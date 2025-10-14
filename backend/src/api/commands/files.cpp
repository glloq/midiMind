// ============================================================================
// Fichier: backend/src/api/commands/files.cpp
// Version: 3.0.8 - CORRIGÉ (Harmonisation noms méthodes MidiFileManager)
// Date: 2025-10-13
// ============================================================================
// Description:
//   Handlers pour les commandes de gestion des fichiers MIDI
//
// CORRECTIONS v3.0.8:
//   ✅ getFileMetadata() → getById()
//   ✅ getFileByPath() → getByPath()
//   ✅ listFiles() → getAll() avec filtrage manuel
//   ✅ Toutes les occurrences harmonisées
//   ✅ 100% compatible avec MidiFileManager.h actuel
//
// Commandes implémentées:
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
                
                // ✅ CORRECTION: listFiles() → getAll()
                auto allFiles = fileManager->getAll();
                
                // Filtrer par répertoire si spécifié
                std::vector<MidiFileEntry> files;
                if (directory.empty()) {
                    files = allFiles;
                } else {
                    for (const auto& file : allFiles) {
                        if (file.directory == directory || 
                            file.directory.find(directory) == 0) {
                            files.push_back(file);
                        }
                    }
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
        },
        "List MIDI files in library"
    );
    
    // ========================================================================
    // files.scan - Scanner le répertoire
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
        },
        "Scan directory for MIDI files"
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
                        {"error", "Missing file_id parameter"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                // ✅ CORRECTION: getFileMetadata() → getById()
                auto fileOpt = fileManager->getById(fileId);
                
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
        },
        "Get file metadata"
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
                        {"error", "Missing filename or data parameter"}
                    };
                }
                
                std::string filename = params["filename"];
                std::string base64Data = params["data"];
                
                std::string fileId = fileManager->uploadFile(filename, base64Data);
                
                Logger::info("FileAPI", 
                    "✓ File uploaded: " + fileId);
                
                return {
                    {"success", true},
                    {"data", {
                        {"file_id", fileId},
                        {"filename", filename}
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
        },
        "Upload MIDI file"
    );
    
    // ========================================================================
    // files.download - Télécharger un fichier
    // ========================================================================
    factory.registerCommand("files.download",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Downloading file...");
            
            try {
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing file_id parameter"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                // ✅ CORRECTION: getFileMetadata() → getById()
                auto fileOpt = fileManager->getById(fileId);
                
                if (!fileOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "File not found"},
                        {"error_code", "FILE_NOT_FOUND"}
                    };
                }
                
                const auto& file = fileOpt.value();
                
                // Lire le fichier en base64
                std::ifstream fileStream(file.filepath, std::ios::binary);
                if (!fileStream) {
                    return {
                        {"success", false},
                        {"error", "Failed to read file"},
                        {"error_code", "READ_FAILED"}
                    };
                }
                
                std::vector<uint8_t> buffer(
                    std::istreambuf_iterator<char>(fileStream), {});
                fileStream.close();
                
                // Encoder en base64 (simplifié pour l'exemple)
                // TODO: Utiliser bibliothèque base64 robuste
                std::string base64Data = ""; // base64_encode(buffer);
                
                Logger::info("FileAPI", 
                    "✓ File downloaded: " + file.filename);
                
                return {
                    {"success", true},
                    {"data", {
                        {"filename", file.filename},
                        {"file_data", base64Data},
                        {"size_bytes", file.fileSizeBytes}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("FileAPI", 
                    "Failed to download file: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "DOWNLOAD_FAILED"}
                };
            }
        },
        "Download MIDI file"
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
                        {"error", "Missing file_id parameter"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                // Vérifier que le fichier existe
                // ✅ CORRECTION: getFileMetadata() → getById()
                auto fileOpt = fileManager->getById(fileId);
                if (!fileOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "File not found"},
                        {"error_code", "FILE_NOT_FOUND"}
                    };
                }
                
                bool success = fileManager->deleteFile(fileId);
                
                if (!success) {
                    return {
                        {"success", false},
                        {"error", "Failed to delete file"},
                        {"error_code", "DELETE_FAILED"}
                    };
                }
                
                Logger::info("FileAPI", 
                    "✓ File deleted: " + fileId);
                
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
        },
        "Delete MIDI file"
    );
    
    // ========================================================================
    // files.rename - Renommer un fichier
    // ========================================================================
    factory.registerCommand("files.rename",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Renaming file...");
            
            try {
                if (!params.contains("file_path") || !params.contains("new_name")) {
                    return {
                        {"success", false},
                        {"error", "Missing file_path or new_name parameter"}
                    };
                }
                
                std::string filePath = params["file_path"];
                std::string newName = params["new_name"];
                
                // Vérifier que le fichier existe
                // ✅ CORRECTION: getFileByPath() → getByPath()
                auto fileOpt = fileManager->getByPath(filePath);
                if (!fileOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "File not found"},
                        {"error_code", "FILE_NOT_FOUND"}
                    };
                }
                
                auto newPathOpt = fileManager->renameFile(filePath, newName);
                
                if (!newPathOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "Failed to rename file"},
                        {"error_code", "RENAME_FAILED"}
                    };
                }
                
                Logger::info("FileAPI", 
                    "✓ File renamed: " + filePath + " -> " + newPathOpt.value());
                
                return {
                    {"success", true},
                    {"message", "File renamed successfully"},
                    {"data", {
                        {"old_path", filePath},
                        {"new_path", newPathOpt.value()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("FileAPI", 
                    "Failed to rename file: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "RENAME_FAILED"}
                };
            }
        },
        "Rename MIDI file"
    );
    
    // ========================================================================
    // files.move - Déplacer un fichier (COMPLET avec DB + cache update)
    // ========================================================================
    factory.registerCommand("files.move",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Moving file...");
            
            try {
                if (!params.contains("file_path") || !params.contains("destination")) {
                    return {
                        {"success", false},
                        {"error", "Missing file_path or destination parameter"}
                    };
                }
                
                std::string filePath = params["file_path"];
                std::string destination = params["destination"];
                
                // Vérifier que le fichier existe
                // ✅ CORRECTION: getFileByPath() → getByPath()
                auto fileOpt = fileManager->getByPath(filePath);
                if (!fileOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "File not found"},
                        {"error_code", "FILE_NOT_FOUND"}
                    };
                }
                
                const auto& file = fileOpt.value();
                
                // Déplacer le fichier (gère déjà BDD et cache)
                auto newPathOpt = fileManager->moveFile(file.id, destination);
                
                if (!newPathOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "Failed to move file"},
                        {"error_code", "MOVE_FAILED"}
                    };
                }
                
                Logger::info("FileAPI", 
                    "✓ File moved: " + filePath + " -> " + newPathOpt.value());
                
                return {
                    {"success", true},
                    {"message", "File moved successfully"},
                    {"data", {
                        {"old_path", filePath},
                        {"new_path", newPathOpt.value()}
                    }}
                };
                
            } catch (const std::exception& e) {
                Logger::error("FileAPI", 
                    "Failed to move file: " + std::string(e.what()));
                return {
                    {"success", false},
                    {"error", e.what()},
                    {"error_code", "MOVE_FAILED"}
                };
            }
        },
        "Move MIDI file"
    );
    
    // ========================================================================
    // files.analyze - Analyser la structure MIDI
    // ========================================================================
    factory.registerCommand("files.analyze",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Analyzing file...");
            
            try {
                if (!params.contains("file_id")) {
                    return {
                        {"success", false},
                        {"error", "Missing file_id parameter"}
                    };
                }
                
                std::string fileId = params["file_id"];
                
                // ✅ CORRECTION: getFileMetadata() → getById()
                auto fileOpt = fileManager->getById(fileId);
                if (!fileOpt.has_value()) {
                    return {
                        {"success", false},
                        {"error", "File not found"},
                        {"error_code", "FILE_NOT_FOUND"}
                    };
                }
                
                // Charger en JsonMidi pour analyse détaillée
                json jsonMidi = fileManager->loadAsJsonMidi(fileId);
                
                Logger::info("FileAPI", "✓ File analyzed");
                
                return {
                    {"success", true},
                    {"data", jsonMidi}
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
        },
        "Analyze MIDI file structure"
    );
    
    // ========================================================================
    // files.search - Rechercher des fichiers
    // ========================================================================
    factory.registerCommand("files.search",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Searching files...");
            
            try {
                if (!params.contains("query")) {
                    return {
                        {"success", false},
                        {"error", "Missing query parameter"}
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
        },
        "Search MIDI files"
    );
    
    // ========================================================================
    // files.updateTags - Mettre à jour les tags
    // ========================================================================
    factory.registerCommand("files.updateTags",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Updating tags...");
            
            try {
                if (!params.contains("file_id") || !params.contains("tags")) {
                    return {
                        {"success", false},
                        {"error", "Missing file_id or tags parameter"}
                    };
                }
                
                std::string fileId = params["file_id"];
                std::vector<std::string> tags = params["tags"].get<std::vector<std::string>>();
                
                // Vérifier que le fichier existe
                // ✅ CORRECTION: getFileMetadata() → getById()
                auto fileOpt = fileManager->getById(fileId);
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
        },
        "Update file tags"
    );
    
    // ========================================================================
    // files.updateRating - Mettre à jour la note
    // ========================================================================
    factory.registerCommand("files.updateRating",
        [fileManager](const json& params) -> json {
            Logger::debug("FileAPI", "Updating rating...");
            
            try {
                if (!params.contains("file_id") || !params.contains("rating")) {
                    return {
                        {"success", false},
                        {"error", "Missing file_id or rating parameter"}
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
                // ✅ CORRECTION: getFileMetadata() → getById()
                auto fileOpt = fileManager->getById(fileId);
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
        },
        "Update file rating"
    );
    
    Logger::info("FileHandlers", "✓ All file commands registered");
}

} // namespace midiMind

// ============================================================================
// FIN DU FICHIER files.cpp v3.0.8 - CORRIGÉ
// ============================================================================