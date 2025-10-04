// ============================================================================
// src/api/commands/library/LibraryCommands.h
// Toutes les commandes de gestion de la bibliothèque MIDI
// ============================================================================
#pragma once

#include "../../../core/commands/BaseCommand.h"
#include "../../../midi/MidiFileManager.h"

namespace midiMind {

// ============================================================================
// LIBRARY SCAN COMMAND
// ============================================================================

class LibraryScanCommand : public BaseCommand {
public:
    LibraryScanCommand(const json& params, std::shared_ptr<MidiFileManager> fileManager)
        : BaseCommand(params), fileManager_(fileManager) {}
    
    std::string getName() const override { return "library.scan"; }
    std::string getDescription() const override {
        return "Scan filesystem for MIDI files and update library";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "recursive"}, {"type", "boolean"}, {"required", false},
             {"default", true}, {"description", "Scan subdirectories"}},
            {{"name", "update_existing"}, {"type", "boolean"}, {"required", false},
             {"default", false}, {"description", "Re-analyze existing files"}}
        });
    }
    
    bool validate(std::string& error) const override {
        // Vérifier si un scan est déjà en cours
        if (fileManager_->isScanning()) {
            error = "A scan is already in progress";
            return false;
        }
        return true;
    }
    
    json execute() override {
        bool recursive = getOptional("recursive", true);
        bool updateExisting = getOptional("update_existing", false);
        
        // Lancer le scan asynchrone
        fileManager_->scanLibrary(recursive, updateExisting);
        
        json response = jsonSuccess("Library scan started");
        response["recursive"] = recursive;
        response["update_existing"] = updateExisting;
        
        return response;
    }

private:
    std::shared_ptr<MidiFileManager> fileManager_;
};

// ============================================================================
// LIBRARY LIST COMMAND
// ============================================================================

class LibraryListCommand : public BaseCommand {
public:
    LibraryListCommand(const json& params, std::shared_ptr<MidiFileManager> fileManager)
        : BaseCommand(params), fileManager_(fileManager) {}
    
    std::string getName() const override { return "library.list"; }
    std::string getDescription() const override {
        return "List MIDI files in library with pagination";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "limit"}, {"type", "integer"}, {"required", false},
             {"default", 100}, {"description", "Maximum files to return (1-500)"}},
            {{"name", "offset"}, {"type", "integer"}, {"required", false},
             {"default", 0}, {"description", "Number of files to skip"}}
        });
    }
    
    bool validate(std::string& error) const override {
        int limit = getOptional("limit", 100);
        int offset = getOptional("offset", 0);
        
        if (limit < 1 || limit > 500) {
            error = "Limit must be between 1 and 500";
            return false;
        }
        
        if (offset < 0) {
            error = "Offset must be >= 0";
            return false;
        }
        
        return true;
    }
    
    json execute() override {
        int limit = getOptional("limit", 100);
        int offset = getOptional("offset", 0);
        
        auto files = fileManager_->listFiles(limit, offset);
        
        json response = jsonSuccess();
        response["files"] = json::array();
        
        for (const auto& file : files) {
            response["files"].push_back(file.toJson());
        }
        
        response["count"] = files.size();
        response["limit"] = limit;
        response["offset"] = offset;
        
        return response;
    }

private:
    std::shared_ptr<MidiFileManager> fileManager_;
};

// ============================================================================
// LIBRARY SEARCH COMMAND
// ============================================================================

class LibrarySearchCommand : public BaseCommand {
public:
    LibrarySearchCommand(const json& params, std::shared_ptr<MidiFileManager> fileManager)
        : BaseCommand(params), fileManager_(fileManager) {}
    
    std::string getName() const override { return "library.search"; }
    std::string getDescription() const override {
        return "Search for MIDI files by name, composer, or tags";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "query"}, {"type", "string"}, {"required", true},
             {"description", "Search query (min 2 characters)"}}
        });
    }
    
    bool validate(std::string& error) const override {
        if (!validateString("query", 200, error)) {
            return false;
        }
        
        std::string query = params_["query"].get<std::string>();
        if (query.length() < 2) {
            error = "Search query must be at least 2 characters";
            return false;
        }
        
        return true;
    }
    
    json execute() override {
        std::string query = params_["query"];
        
        auto files = fileManager_->searchFiles(query);
        
        json response = jsonSuccess();
        response["query"] = query;
        response["results"] = json::array();
        
        for (const auto& file : files) {
            response["results"].push_back(file.toJson());
        }
        
        response["count"] = files.size();
        
        return response;
    }

private:
    std::shared_ptr<MidiFileManager> fileManager_;
};

// ============================================================================
// LIBRARY GET COMMAND
// ============================================================================

class LibraryGetCommand : public BaseCommand {
public:
    LibraryGetCommand(const json& params, std::shared_ptr<MidiFileManager> fileManager)
        : BaseCommand(params), fileManager_(fileManager) {}
    
    std::string getName() const override { return "library.get"; }
    std::string getDescription() const override {
        return "Get detailed information about a specific MIDI file";
    }
    
    json getParameterSpec() const override {
        return json::array({
            {{"name", "file_id"}, {"type", "string"}, {"required", true},
             {"description", "Unique identifier of the file"}}
        });
    }
    
    bool validate(std::string& error) const override {
        return validateString("file_id", 100, error);
    }
    
    json execute() override {
        std::string fileId = params_["file_id"];
        
        auto file = fileManager_->getFile(fileId);
        
        if (!file) {
            return jsonError("File not found: " + fileId);
        }
        
        json response = jsonSuccess();
        response["file"] = file->toJson();
        
        return response;
    }

private:
    std::shared_ptr<MidiFileManager> fileManager_;
};

// ============================================================================
// LIBRARY STATS COMMAND
// ============================================================================

class LibraryStatsCommand : public BaseCommand {
public:
    LibraryStatsCommand(const json& params, std::shared_ptr<MidiFileManager> fileManager)
        : BaseCommand(params), fileManager_(fileManager) {}
    
    std::string getName() const override { return "library.stats"; }
    std::string getDescription() const override {
        return "Get statistics about the MIDI library";
    }
    
    bool validate(std::string& error) const override {
        return true; // Pas de paramètres
    }
    
    json execute() override {
        auto stats = fileManager_->getStatistics();
        
        json response = jsonSuccess();
        response["statistics"] = stats;
        
        return response;
    }

private:
    std::shared_ptr<MidiFileManager> fileManager_;
};

} // namespace midiMind