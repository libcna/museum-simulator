// Pipeline laboratory: the XNA content pipeline running inside the game. A small content type of the museum's own is
// described to CNA's IntermediateSerializer, written out as the XML a content project would hold, and read back; a
// damaged copy of the document shows what the pipeline refuses and the message it gives. This is the build-time half of
// ContentManager's story (the Content Compiler room shows the other half, the finished .xnb).
// Without CNA's pipeline library (-DMUSEUM_PIPELINE=OFF) the room says so.
#include <cstdio>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "Exhibits/ExhibitFactories.hpp"
#include "Exhibits/Content/PipelineSupport.hpp"
#include "Exhibits/ScreenExhibit.hpp"

#ifdef MUSEUM_PIPELINE
#include "Microsoft/Xna/Framework/Color.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/ContentItem.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/InvalidContentException.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/Serialization/Intermediate/IntermediateSerializer.hpp"
#include "Microsoft/Xna/Framework/Content/Pipeline/XmlImporter.hpp"
#include "Microsoft/Xna/Framework/Content/ContentSerializerCollectionItemNameAttribute.hpp"
#include "Microsoft/Xna/Framework/Vector3.hpp"
#include "System/Xml/XmlReader.hpp"
#include "System/Xml/XmlWriter.hpp"
#include "System/Xml/XmlWriterSettings.hpp"
#endif

using namespace Microsoft::Xna::Framework;
using namespace Microsoft::Xna::Framework::Graphics;

namespace {

#ifdef MUSEUM_PIPELINE
namespace Xna = Microsoft::Xna::Framework::Content::Pipeline;
namespace Intermediate = Microsoft::Xna::Framework::Content::Pipeline::Serialization::Intermediate;

// A museum plaque as content: what a game would author as an .xml file and compile to an .xnb.
class Plaque final : public Xna::ContentItem {
public:
    static constexpr std::string_view XnaTypeName = "Museum.Plaque";
    Vector3 position{1.5f, 0.0f, -2.0f};
    Color tint{255, 214, 120, 255};
    std::string text = "CNA 3D Museum";
    std::int32_t level = 3;
    bool pinned = true;
    std::vector<std::string> tags{"gallery", "gold"};

    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name{XnaTypeName};
        return name;
    }
    static void DescribeContent(Intermediate::ContentTypeDescriptor<Plaque>& d)
    {
        d.Field("Position", &Plaque::position);
        d.Field("Tint", &Plaque::tint);
        d.Field("Text", &Plaque::text);
        d.Field("Level", &Plaque::level);
        d.Field("Pinned", &Plaque::pinned);
        d.Field("Tags", &Plaque::tags);
    }
};

// The same plaque described with the options XNA writes as [ContentSerializer...] attributes on members: a different element name,
// a collection item name, Optional (left out while the collection is empty; reading a file without it keeps the default), Ignore (never written).
class AnnotatedPlaque final : public Xna::ContentItem {
public:
    static constexpr std::string_view XnaTypeName = "Museum.AnnotatedPlaque";
    Vector3 position{1.5f, 0.0f, -2.0f};
    Color tint{255, 214, 120, 255};
    std::string text = "CNA 3D Museum";
    std::int32_t level = 3;
    bool pinned = true;
    std::vector<std::string> tags{"gallery", "gold"};
    std::string secret = "visitor's notes never reach the file";

    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name{XnaTypeName};
        return name;
    }
    static void DescribeContent(Intermediate::ContentTypeDescriptor<AnnotatedPlaque>& d)
    {
        d.Field("Position", &AnnotatedPlaque::position).ElementName("Pos");                    // [ContentSerializer(ElementName = "Pos")]
        d.Field("Tint", &AnnotatedPlaque::tint);
        d.Field("Text", &AnnotatedPlaque::text).ElementName("Caption");
        d.Field("Level", &AnnotatedPlaque::level);
        d.Field("Pinned", &AnnotatedPlaque::pinned);
        // Optional: left out of the file while the collection is empty, and a file without it keeps the constructor's two tags.
        d.Field("Tags", &AnnotatedPlaque::tags).CollectionItemName("Tag").Optional();          // [ContentSerializer(CollectionItemName = "Tag", Optional = true)]
        d.Field("Secret", &AnnotatedPlaque::secret).Ignore();                                  // [ContentSerializerIgnore]
    }
};

// XNA exposes IntermediateSerializer.GetTypeSerializer to custom ContentTypeSerializer classes:
// the framework owns the serializer instance and hands it to their IntermediateReader/Writer.
class SerializerProbe final : public Xna::ContentItem {
public:
    static constexpr std::string_view XnaTypeName = "Museum.SerializerProbe";
    std::int32_t value = 42;

    [[nodiscard]] const std::string& GetTypeName() const override
    {
        static const std::string name{XnaTypeName};
        return name;
    }
};

std::string serializerProbeResult = "-";

class SerializerProbeSerializer final : public Intermediate::ContentTypeSerializer<SerializerProbe> {
protected:
    void Serialize(Intermediate::IntermediateWriter& output, const std::shared_ptr<SerializerProbe>& value,
                   const Content::ContentSerializerAttribute& format) override
    {
        (void)output.getSerializerProperty().GetTypeSerializer(System::Type::From<std::int32_t>());
        serializerProbeResult = "writer resolved Int32";
        output.WriteRawObject<std::int32_t>(value->value, format);
    }

    [[nodiscard]] std::shared_ptr<SerializerProbe> Deserialize(
        Intermediate::IntermediateReader& input, const Content::ContentSerializerAttribute& format,
        std::shared_ptr<SerializerProbe> existing) override
    {
        (void)input.getSerializerProperty().GetTypeSerializer(System::Type::From<std::int32_t>());
        if (!existing) existing = std::make_shared<SerializerProbe>();
        existing->value = input.ReadRawObject<std::int32_t>(format);
        serializerProbeResult += "; reader resolved Int32";
        return existing;
    }
};

template <typename T>
std::string ToXml(const std::shared_ptr<T>& value)
{
    System::Xml::XmlWriterSettings settings;
    settings.Indent = true;
    settings.NewLineChars = "\n";
    std::unique_ptr<System::Xml::XmlWriter> writer(System::Xml::XmlWriter::CreateToString(settings));
    Intermediate::IntermediateSerializer::Serialize<T>(*writer, value, std::string());
    return writer->ToString();
}

template <typename T>
std::shared_ptr<T> FromXml(const std::string& xml)
{
    std::unique_ptr<System::Xml::XmlReader> reader(System::Xml::XmlReader::CreateFromString(xml));
    return Intermediate::IntermediateSerializer::Deserialize<T>(*reader, std::string());
}

std::vector<std::string> Lines(const std::string& text)
{
    std::vector<std::string> out;
    std::istringstream in(text);
    for (std::string line; std::getline(in, line);) out.push_back(line);
    return out;
}
#endif

class PipelineLab final : public ScreenExhibit {
public:
    PipelineLab(const RoomDef& room, Services& services) : ScreenExhibit(room, services, 960, 540, 7.6f) {}

    void BuildStatic(RoomBuilder& b) override
    {
        ControlPanel& p = AddPanel("Intermediate XML", -4.8f, 1.5f, 1.8f, MathHelper::PiOver2, 2.1f);
#ifdef MUSEUM_PIPELINE
        p.AddSlider("Position.X", -5.0f, 5.0f, 1.5f, [this](float v) { item_->position.X = v; Rebuild(); }, "%.1f");
        p.AddSlider("Position.Z", -5.0f, 5.0f, -2.0f, [this](float v) { item_->position.Z = v; Rebuild(); }, "%.1f");
        p.AddChoice("Tint", {"gold", "sky", "rose"}, 0, [this](int v) {
            static const Color tints[] = {Color(255, 214, 120, 255), Color(120, 190, 255, 255), Color(255, 140, 170, 255)};
            item_->tint = tints[v];
            Rebuild();
        });
        p.AddChoice("Text", {"CNA 3D Museum", "Mind the <step> & the \"rail\"", "Käsekuchen: 日本語"}, 0, [this](int v) {
            static const char* texts[] = {"CNA 3D Museum", "Mind the <step> & the \"rail\"", "K\xC3\xA4sekuchen: \xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E"};
            item_->text = texts[v];
            Rebuild();
        });
        p.AddSlider("Level", 1.0f, 9.0f, 3.0f, [this](float v) { item_->level = static_cast<std::int32_t>(v); Rebuild(); }, "%.0f", 1.0f);
        p.AddToggle("Pinned", true, [this](bool v) { item_->pinned = v; Rebuild(); });
        p.AddSlider("Tags", 0.0f, 4.0f, 2.0f, [this](float v) {
            static const char* all[] = {"gallery", "gold", "plaque", "xna"};
            item_->tags.assign(all, all + static_cast<int>(v));
            Rebuild();
        }, "%.0f", 1.0f);
        p.AddChoice("Members described with", {"plain fields", "options: ElementName, CollectionItemName, Optional, Ignore"}, 0, [this](int v) { annotated_ = v == 1; Rebuild(); });
        p.AddReadout("Ignored member in the file", [this] { return secretInFile_; });
        p.AddChoice("Damage the document", {"none", "unknown element", "number that is not a number", "missing end tag", "wrong type name"}, 0, [this](int v) { damage_ = v; Rebuild(); });
        p.AddReadout("IntermediateSerializer.Serialize", [this] { return std::to_string(xml_.size()) + " bytes"; });
        p.AddReadout("GetTypeSerializer / collection item attribute", [this] { return serializer_; });
        p.AddReadout("XmlImporter.Attribute", [this] { return importerAttribute_; });
        p.AddReadout("XmlImporter.Import", [this] { return imported_; });
        p.AddReadout("Deserialize", [this] { return verdict_; });
        p.AddReadout("Round trip", [this] { return roundTrip_; });
#else
        p.AddReadout("Content pipeline", [] { return std::string("not built into this museum"); });
#endif
        AddInfoSign(b, "The Pipeline in the Game",
                    {"XNA games author content as XML (or import files), and the content pipeline turns it into the .xnb that ContentManager loads. The IntermediateSerializer is the XML half:",
                     "a type says which members it has (DescribeContent), and CNA writes and reads the very format XNA's did. XmlImporter consumes that authored .xml file and returns the type named by its Asset element; unlike most importers it declares no default processor.",
                     "Describe the members with options (the [ContentSerializer...] attributes of XNA) or damage the document to see what both reading paths refuse; set Tags to 0 with the options on: an Optional element is omitted, and reading it back restores the default."});
    }

    void OnScreenLoaded() override
    {
#ifdef MUSEUM_PIPELINE
        item_ = std::make_shared<Plaque>();
        static const bool probeRegistered = [] {
            (void)Intermediate::IntermediateSerializer::AddTypeSerializer<SerializerProbeSerializer>();
            return true;
        }();
        (void)probeRegistered;
        serializerProbeResult = "-";
        const std::shared_ptr<SerializerProbe> probe = std::make_shared<SerializerProbe>();
        const std::shared_ptr<SerializerProbe> probeBack = FromXml<SerializerProbe>(ToXml(probe));
        serializer_ = serializerProbeResult + "; value " + std::to_string(probeBack->value);
        const Content::ContentSerializerCollectionItemNameAttribute itemName("Tag");
        serializer_ += "; CollectionItemName = " + itemName.getCollectionItemNameProperty();
        const Xna::ContentImporterAttribute attribute = Xna::XmlImporter::Attribute();
        importerAttribute_ = attribute.getFileExtensionsProperty()[0] + ", \"" +
                             attribute.getDisplayNameProperty() + "\", default processor: " +
                             (attribute.getDefaultProcessorProperty().empty() ? "none" : attribute.getDefaultProcessorProperty());
        Rebuild();
#endif
    }

protected:
    void DrawScreen(SpriteBatch& sb) override
    {
        if (S().font == nullptr) return;
        sb.Begin(SpriteSortMode::Deferred, &BlendState::NonPremultiplied, &SamplerState::LinearClamp, nullptr, nullptr);
        Texture2D& white = *S().whiteTexture;
        const auto rect = [&](float x, float y, float w, float h, Color c) { sb.Draw(white, Rectangle(static_cast<int>(x), static_cast<int>(y), static_cast<int>(w), static_cast<int>(h)), c); };
        const auto text = [&](const std::string& t, float x, float y, Color c, float s) { sb.DrawString(*S().font, t, Vector2(x, y), c, 0.0f, Vector2(0, 0), s, SpriteEffects::None, 0.0f); };
        text("The intermediate XML", 24, 12, Kit::kGold, 1.0f);
#ifdef MUSEUM_PIPELINE
        rect(24, 56, 560, 460, Color(16, 22, 34, 245));
        float y = 66.0f;
        for (const std::string& line : shownLines_) {
            if (y > 500.0f) break;
            text(line, 36, y, damage_ != 0 ? Kit::Rgba(255, 205, 150) : Kit::kText, 0.5f);
            y += 17.0f;
        }
        rect(604, 56, 332, 460, Color(22, 30, 46, 235));
        text("Reading it back", 618, 66, Kit::kGold, 0.75f);
        float ry = 104.0f;
        text("XmlImporter.Import", 618, ry, Kit::kGold, 0.58f);
        ry += 28.0f;
        for (const std::string& line : Kit::Wrap(*S().font, imported_, 300.0f, 0.5f)) {
            text(line, 618, ry, imported_.starts_with("refused") ? Kit::Rgba(255, 180, 150) : Kit::Rgba(150, 255, 190), 0.5f);
            ry += 20.0f;
        }
        ry += 8.0f;
        text("Deserialize directly", 618, ry, Kit::kGold, 0.58f);
        ry += 28.0f;
        for (const std::string& line : Kit::Wrap(*S().font, verdict_, 300.0f, 0.55f)) {
            text(line, 618, ry, ok_ ? Kit::Rgba(150, 255, 190) : Kit::Rgba(255, 180, 150), 0.55f);
            ry += 22.0f;
        }
        ry += 12.0f;
        text("Round trip", 618, ry, Kit::kGold, 0.75f);
        ry += 36.0f;
        for (const std::string& line : Kit::Wrap(*S().font, roundTrip_, 300.0f, 0.55f)) {
            text(line, 618, ry, Kit::kText, 0.55f);
            ry += 22.0f;
        }
#else
        text("This museum was built without CNA's content pipeline library (-DMUSEUM_PIPELINE=OFF).", 24, 90, Kit::Rgba(255, 190, 150), 0.7f);
        (void)rect;
#endif
        sb.End();
    }

private:
#ifdef MUSEUM_PIPELINE
    // Writes the document, optionally damages it, reads it back; everything the screen shows comes from here.
    void Rebuild()
    {
        if (annotated_) {
            auto copy = std::make_shared<AnnotatedPlaque>();
            copy->position = item_->position;
            copy->tint = item_->tint;
            copy->text = item_->text;
            copy->level = item_->level;
            copy->pinned = item_->pinned;
            copy->tags = item_->tags;
            Document<AnnotatedPlaque>(copy);
        } else {
            Document<Plaque>(item_);
        }
    }

    template <typename T>
    std::shared_ptr<T> ImportDocument(const std::string& document)
    {
        const std::filesystem::path directory = std::filesystem::temp_directory_path() / "cna-museum-pipeline";
        std::error_code ignored;
        std::filesystem::create_directories(directory, ignored);
        const std::filesystem::path source = directory /
            ("plaque-" + std::to_string(reinterpret_cast<std::uintptr_t>(this)) + ".xml");
        {
            std::ofstream out(source, std::ios::binary | std::ios::trunc);
            if (!out) throw std::runtime_error("could not create the temporary .xml source");
            out.write(document.data(), static_cast<std::streamsize>(document.size()));
            if (!out) throw std::runtime_error("could not write the temporary .xml source");
        }
        try {
            PipelineSupport::ImporterContext context;
            Xna::XmlImporter importer;
            const Xna::ContentObject value = importer.Import(source.string(), context);
            std::filesystem::remove(source, ignored);
            return Xna::Unbox<std::shared_ptr<T>>(value);
        } catch (...) {
            std::filesystem::remove(source, ignored);
            throw;
        }
    }

    template <typename T>
    void Document(const std::shared_ptr<T>& item)
    {
        const bool annotated = std::is_same_v<T, AnnotatedPlaque>;
        try {
            xml_ = ToXml(item);
            std::string doc = xml_;
            const auto replace = [&](const std::string& a, const std::string& b) {
                const std::size_t at = doc.find(a);
                if (at != std::string::npos) doc.replace(at, a.size(), b);
            };
            switch (damage_) {
                case 1:
                    if (annotated) { replace("<Caption>", "<Captin>"); replace("</Caption>", "</Captin>"); }
                    else { replace("<Level>", "<Levl>"); replace("</Level>", "</Levl>"); }
                    break;
                case 2:
                    if (annotated) replace("<Pos>", "<Pos>x");
                    else replace(std::to_string(item->level) + "</Level>", "three</Level>");
                    break;
                case 3: replace(annotated ? "</Caption>" : "</Pinned>", ""); break;
                case 4: replace(annotated ? "Museum:AnnotatedPlaque\"" : "Museum:Plaque\"", "Museum:Plaquette\""); break;
                default: break;
            }
            shownLines_ = Lines(doc);
            secretInFile_ = doc.find("never reach") != std::string::npos ? "yes" : "no: [ContentSerializerIgnore]";
            try {
                const std::shared_ptr<T> imported = ImportDocument<T>(doc);
                imported_ = std::string("returned ") + (annotated ? "AnnotatedPlaque" : "Plaque") +
                            ": \"" + imported->text + "\", level " + std::to_string(imported->level);
            } catch (const std::exception& e) {
                imported_ = std::string("refused: ") + e.what();
            }
            try {
                const std::shared_ptr<T> back = FromXml<T>(doc);
                ok_ = true;
                verdict_ = std::string("Deserialize returned a ") + (annotated ? "AnnotatedPlaque" : "Plaque") + ": \"" + back->text + "\", level " + std::to_string(back->level) + ", " + std::to_string(back->tags.size()) + " tags";
                roundTrip_ = ToXml(back) == xml_ ? "writing it again gives the same document, byte for byte" : "the second document differs";
            } catch (const std::exception& e) {
                ok_ = false;
                verdict_ = std::string("refused: ") + e.what();
                roundTrip_ = "nothing to write back";
            }
        } catch (const std::exception& e) {
            ok_ = false;
            verdict_ = std::string("failed: ") + e.what();
        }
    }

    std::shared_ptr<Plaque> item_;
    std::string xml_;
    std::vector<std::string> shownLines_;
    std::string verdict_ = "-", roundTrip_ = "-", imported_ = "-", importerAttribute_ = "-", serializer_ = "-";
    bool ok_ = true, annotated_ = false;
    int damage_ = 0;
    std::string secretInFile_ = "-";
#endif
};

}  // namespace

std::unique_ptr<Exhibit> MakePipelineLab(const RoomDef& room, Services& services)
{
    return std::make_unique<PipelineLab>(room, services);
}
