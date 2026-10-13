#include "World/Progress.hpp"

#include <exception>
#include <sstream>
#include <vector>

#include "Diagnostics/Log.hpp"
#include "Microsoft/Xna/Framework/Storage/StorageContainer.hpp"
#include "Microsoft/Xna/Framework/Storage/StorageDevice.hpp"
#include "System/IO/Stream.hpp"

using namespace Microsoft::Xna::Framework::Storage;

namespace {
constexpr const char* kFile = "progress.txt";
constexpr const char* kHeader = "cna-3d-museum progress 1";
}  // namespace

struct ProgressStore::Impl {
    std::unique_ptr<StorageDevice> device;
    std::unique_ptr<StorageContainer> container;
};

ProgressStore::ProgressStore(bool enabled) : impl_(std::make_unique<Impl>()), enabled_(enabled) {}
ProgressStore::~ProgressStore() = default;

bool ProgressStore::Open()
{
    if (impl_->container) return true;
    try {
        StorageDevice::SetAppNameEXT("cna-3d-museum");
        auto selector = StorageDevice::BeginShowSelector(nullptr, nullptr);
        impl_->device = StorageDevice::EndShowSelector(selector.get());
        auto open = impl_->device->BeginOpenContainer("Museum", nullptr, nullptr);
        impl_->container = impl_->device->EndOpenContainer(open.get());
        where_ = StorageDevice::GetStorageRootEXT();
        return true;
    } catch (const std::exception& e) {
        Museum::Log("progress", "storage unavailable (%s): progress will not be saved", e.what());
        enabled_ = false;
        return false;
    }
}

std::set<int> ProgressStore::Load(const MuseumPlan& museum)
{
    std::set<int> visited;
    if (!enabled_ || !Open()) return visited;
    try {
        if (!impl_->container->FileExists(kFile)) return visited;
        auto stream = impl_->container->OpenFile(kFile, System::IO::FileMode::Open);
        std::vector<SharpRuntime::bytecs> bytes(static_cast<std::size_t>(stream->getLengthProperty()));
        const int n = stream->Read(bytes.data(), 0, static_cast<int>(bytes.size()));
        stream->Close();
        std::istringstream in(std::string(bytes.begin(), bytes.begin() + n));
        std::string line;
        if (!std::getline(in, line) || line != kHeader) {
            Museum::Log("progress", "ignoring a progress file with an unknown header");
            return visited;
        }
        while (std::getline(in, line))
            if (const RoomDef* r = museum.FindByKey(line)) visited.insert(r->index);
    } catch (const std::exception& e) {
        Museum::Log("progress", "could not read progress (%s)", e.what());
        visited.clear();
    }
    return visited;
}

bool ProgressStore::Save(const std::set<int>& visited, const MuseumPlan& museum)
{
    if (!enabled_ || !Open()) return false;
    try {
        std::ostringstream out;
        out << kHeader << "\n";
        for (const int cell : visited) out << museum.Room(cell).key << "\n";
        const std::string body = out.str();
        auto stream = impl_->container->CreateFile(kFile);
        std::vector<SharpRuntime::bytecs> bytes(body.begin(), body.end());
        stream->Write(bytes.data(), 0, static_cast<int>(bytes.size()));
        stream->Close();
        return true;
    } catch (const std::exception& e) {
        Museum::Log("progress", "could not save progress (%s)", e.what());
        return false;
    }
}
