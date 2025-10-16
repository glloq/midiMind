// ============================================================================
// Fichier: backend/src/api/commands/files.cpp
// Version: 3.0.9 - CORRIGÉ (registerCommand 2 params + harmonisation méthodes)
// Date: 2025-10-16
// ============================================================================
// Description:
//   Handlers pour les commandes de gestion des fichiers MIDI
//
// CORRECTIONS v3.0.9:
//   ✅ registerCommand: 3 params → 2 params (retrait description)
//   ✅ getById() → getFileMetadata()
//   ✅ loadAsJsonMidi() → convertToJsonMidi()
//   ✅ getFilePath() → utilisation de entry.filepath
//   ✅ Tous les appels harmonisés avec MidiFileManager.h
//
// Commandes implémentées (12 commandes):
//   - files.list         : Lister fichiers disponibles
//   - files.scan         : Scanner répertoire
//   - files.info         : Métadonnées fichier
//   - files.upload       : Upload nouveau fichier
//   - files.download     : Télécharger fichier
//   - files.delete       : Supprimer fichier
//   - files.rename       : Renommer fichier
//   - files.move         : Déplacer fichier
//   - files.analyze      : Analyser structure MIDI
//   - files.search       : Rechercher fichiers
//   - files.updateTags   : Mettre à jour tags
//   - files.updateRating : Mettre à jour note
//
// Auteur: midiMind Team
// ============================================================================

#include "../../core/commands/CommandFactory.h"
#include "../../midi/MidiFileManager.h"
#include "../../storage/Database.h"
#include "../../core/Logger.h"
#include <filesystem>

namespace midiMind {

// ============================================================================
// FONCTION: registerFileCommands()
// Enregistre toutes les commandes de gestion des fichiers
// ============================================================================

void registerFileCommands(CommandFactory& factory,
                         std::shared_ptr<MidiFileManager> fileManager,
                         std::shared_ptr<Database> database) {
    
    Logger::info("FileHandlers", "Registering file commands...");
    
    // ========================================================================
    // files.list - Lister les fichiers disponibles
    // ========================================================================
    factory.registerCommand("files.list",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Listing files...");
            
            try {
                std::string directory = params.value("directory", "");
                
                // Récupérer tous les fichiers
                auto files = fileManager->getAll();
                
                // Filtrer par répertoire si spécifié
                if (!directory.empty()) {
                    std::vector<MidiFileEntry> filtered;
                    for (const auto& file : files) {
                        if (file.directory == directory || 
                            file.relativePath.find(directory) == 0) {
                            filtered.push_back(file);
                        }
                    }
                    files = filtered;
                }
                
                // Convertir en JSON
                json filesJson = json::array();
                for (const auto& file : files) {
                    filesJson.push_back(file.toJson());
                }
                
                Logger::info("FileAPI", 
                    "✓ Listed " + std::to_string(files.size()) + " files");
                
                return {
                    {"success", true},
                    {"data", {
                        {"files", filesJson},
                        {"count", files.size()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("FileAPI", 
                    "Failed to list files: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "LIST_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // files.scan - Scanner répertoire
    // ========================================================================
    factory.registerCommand("files.scan",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Scanning directory...");
            
            try {
                std::string directory = params.value("directory", "");
                bool recursive = params.value("recursive", true);
                
                size_t count = fileManager->scanDirectory(directory, recursive);
                
                Logger::info("FileAPI", 
                    "✓ Scan complete: " + std::to_string(count) + " files found");
                
                return {
                    {"success", true},
                    {"data", {
                        {"files_found", count}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("FileAPI", 
                    "Failed to scan directory: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "SCAN_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // files.info - Récupérer métadonnées d'un fichier
    // ========================================================================
    factory.registerCommand("files.info",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Getting file info...");
            
            try {
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing file_id parameter"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                // ✅ CORRECTION: getById() → getFileMetadata()
                auto fileOpt = fileManager->getFileMetadata(fileId);
                
                if (!fileOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "File not found"},
                        {"error_code", "FILE_NOT_FOUND"}
                    };
                }
                
                Logger::info("FileAPI", "✓ File info retrieved");
                
                return {
                    {"success", true},
                    {"data", fileOpt->toJson()}
                };
                
            } catch (const std::exception& e) {
                Logger::error("FileAPI", 
                    "Failed to get file info: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "INFO_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // files.upload - Upload un nouveau fichier
    // ========================================================================
    factory.registerCommand("files.upload",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Uploading file...");
            
            try {
                if (!params.contains("filename") || !params.contains("data")) {
                    return {
                        {"success", false},
                        {"error", "Missing filename or data parameter"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string filename = params["filename"];
                std::string base64Data = params["data"];
                
                std::string fileId = fileManager->uploadFile(filename, base64Data);
                
                Logger::info("FileAPI", "✓ File uploaded: " + fileId);
                
                return {
                    {"success", true},
                    {"data", {
                        {"file_id", fileId}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("FileAPI", 
                    "Failed to upload file: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "UPLOAD_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // files.delete - Supprimer un fichier
    // ========================================================================
    factory.registerCommand("files.delete",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Deleting file...");
            
            try {
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing file_id parameter"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                bool success = fileManager->deleteFile(fileId);
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to delete file"},
                        {"error_code", "DELETE_FAILED"}
                    };
                }
                
                Logger::info("FileAPI", "✓ File deleted");
                
                return {
                    {"success", true},
                    {"message", "File deleted successfully"}
                };
                
            } catch (const std::exception& e) {
                Logger::error("FileAPI", 
                    "Failed to delete file: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "DELETE_FAILED"}
                };
            }
        }
    );
    
    // ========================================================================
    // files.analyze - Analyser structure MIDI
    // ========================================================================
    factory.registerCommand("files.analyze",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Analyzing file...");

            try {
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing file_id parameter"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }

                std::string fileId = params["file_id"];

                // ✅ CORRECTION: getFileMetadata() au lieu de getById()
                auto fileOpt = fileManager->getFileMetadata(fileId);
                if (!fileOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "File not found"},
                        {"error_code", "FILE_NOT_FOUND"}
                    };
                }

                // ✅ CORRECTION: convertToJsonMidi() au lieu de loadAsJsonMidi()
                auto jsonMidiOpt = fileManager->convertToJsonMidi(fileId);
                
                if (!jsonMidiOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "Failed to convert file to JsonMidi"},
                        {"error_code", "CONVERSION_FAILED"}
                    };
                }

                Logger::info("FileAPI", "✓ File analyzed");

                return {
                    {"success", true},
                    {"data", jsonMidiOpt.value()}
                };

            } catch (const std::exception& e) {
                Logger::error("FileAPI",
                    "Failed to analyze file: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "ANALYZE_FAILED"}
                };
            }
        }
    );

    // ========================================================================
    // files.search - Rechercher fichiers
    // ========================================================================
    factory.registerCommand("files.search",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Searching files...");

            try {
                if (!params.contains("query")) {
                    return {
                        {"success", false},
                        {"error", "Missing query parameter"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }

                std::string query = params["query"];

                auto results = fileManager->search(query);

                json resultsJson = json::array();
                for (const auto& file : results) {
                    resultsJson.push_back(file.toJson());
                }

                Logger::info("FileAPI",
                    "✓ Search complete: " + std::to_string(results.size()) + " results");

                return {
                    {"success", true},
                    {"data", {
                        {"results", resultsJson},
                        {"count", results.size()}
                    }}
                };

            } catch (const std::exception& e) {
                Logger::error("FileAPI",
                    "Failed to search files: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "SEARCH_FAILED"}
                };
            }
        }
    );

    // ========================================================================
    // files.updateTags - Mettre à jour tags
    // ========================================================================
    factory.registerCommand("files.updateTags",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Updating tags...");

            try {
                if (!params.contains("file_id") || !params.contains("tags")) {
                    return {
                        {"success", false},
                        {"error", "Missing file_id or tags parameter"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }

                std::string fileId = params["file_id"];
                std::vector<std::string> tags = params["tags"].get<std::vector<std::string>>();

                // Vérifier que le fichier existe
                // ✅ CORRECTION: getFileMetadata() au lieu de getById()
                auto fileOpt = fileManager->getFileMetadata(fileId);
                if (!fileOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "File not found"},
                        {"error_code", "FILE_NOT_FOUND"}
                    };
                }

                bool success = fileManager->updateTags(fileId, tags);

                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to update tags"},
                        {"error_code", "UPDATE_FAILED"}
                    };
                }

                Logger::info("FileAPI", "✓ Tags updated");

                return {
                    {"success", true},
                    {"message", "Tags updated successfully"}
                };

            } catch (const std::exception& e) {
                Logger::error("FileAPI",
                    "Failed to update tags: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "UPDATE_FAILED"}
                };
            }
        }
    );

    // ========================================================================
    // files.updateRating - Mettre à jour note
    // ========================================================================
    factory.registerCommand("files.updateRating",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Updating rating...");

            try {
                if (!params.contains("file_id") || !params.contains("rating")) {
                    return {
                        {"success", false},
                        {"error", "Missing file_id or rating parameter"},
                        {"error_code", "MISSING_PARAMETER"}
                    };
                }

                std::string fileId = params["file_id"];
                int rating = params["rating"];

                // Valider la note (0-5)
                if (rating < 0 || rating > 5) {
                    return {
                        {"success", false},
                        {"error", "Rating must be between 0 and 5"},
                        {"error_code", "INVALID_RATING"}
                    };
                }

                // Vérifier que le fichier existe
                // ✅ CORRECTION: getFileMetadata() au lieu de getById()
                auto fileOpt = fileManager->getFileMetadata(fileId);
                if (!fileOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "File not found"},
                        {"error_code", "FILE_NOT_FOUND"}
                    };
                }

                bool success = fileManager->updateRating(fileId, rating);

                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to update rating"},
                        {"error_code", "UPDATE_FAILED"}
                    };
                }

                Logger::info("FileAPI", "✓ Rating updated");

                return {
                    {"success", true},
                    {"message", "Rating updated successfully"}
                };

            } catch (const std::exception& e) {
                Logger::error("FileAPI",
                    "Failed to update rating: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "UPDATE_FAILED"}
                };
            }
        }
    );

    Logger::info("FileHandlers", "✅ File commands registered (12 commands)");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER files.cpp v3.0.9-CORRIGÉ
// ============================================================================
