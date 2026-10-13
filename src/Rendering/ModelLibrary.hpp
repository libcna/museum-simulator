#pragma once

#include <map>
#include <memory>
#include <string>

#include "Microsoft/Xna/Framework/Content/ContentManager.hpp"
#include "Microsoft/Xna/Framework/Graphics/Model.hpp"

/** Models by short name ("torus" -> Content/Models/torus.gltf). A model that fails to load yields null. */
class ModelLibrary {
public:
    explicit ModelLibrary(Microsoft::Xna::Framework::Content::ContentManager& content) : content_(content) {}

    [[nodiscard]] Microsoft::Xna::Framework::Graphics::Model* Get(const std::string& name);
    [[nodiscard]] int FailedCount() const { return failed_; }

private:
    Microsoft::Xna::Framework::Content::ContentManager& content_;
    std::map<std::string, std::unique_ptr<Microsoft::Xna::Framework::Graphics::Model>> models_;
    int failed_ = 0;
};
