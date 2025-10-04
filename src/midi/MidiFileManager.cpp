// ============================================================================
// tests/midi/test_MidiFileManager.cpp
// Tests unitaires pour MidiFileManager
// ============================================================================

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "midi/MidiFileManager.h"
#include <filesystem>
#include <fstream>
#include <thread>

using namespace midiMind;
namespace fs = std::filesystem;

// ============================================================================
// FIXTURES ET HELPERS
// ============================================================================

class MidiFileManagerTestFixture {
public:
    MidiFileManagerTestFixture() {
        // Créer un répertoire temporaire pour les tests
        testDir = fs::temp_directory_path() / "midimind_test";
        dbPath = (testDir / "test.db").string();
        midiDir = (testDir / "midi").string();
        
        // Nettoyer si existe déjà
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
        
        // Créer les répertoires
        fs::create_directories(testDir);
        fs::create_directories(midiDir);
    }
    
    ~MidiFileManagerTestFixture() {
        // Nettoyer après les tests
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }
    
    void createDummyMidiFile(const std::string& filename) {
        // Créer un fichier MIDI minimal valide
        std::string filepath = midiDir + "/" + filename;
        std::ofstream file(filepath, std::ios::binary);
        
        // Header MIDI (MThd)
        file.write("MThd", 4);
        uint32_t headerSize = 0x06000000; // 6 bytes (big endian)
        file.write(reinterpret_cast<const char*>(&headerSize), 4);
        uint16_t format = 0x0000; // Format 0
        file.write(reinterpret_cast<const char*>(&format), 2);
        uint16_t tracks = 0x0100; // 1 track
        file.write(reinterpret_cast<const char*>(&tracks), 2);
        uint16_t division = 0xE001; // 480 ticks/quarter (big endian)
        file.write(reinterpret_cast<const char*>(&division), 2);
        
        // Track (MTrk)
        file.write("MTrk", 4);
        uint32_t trackSize = 0x04000000; // 4 bytes
        file.write(reinterpret_cast<const char*>(&trackSize), 4);
        
        // End of track
        file.write("\x00\xFF\x2F\x00", 4);
        
        file.close();
    }
    
    fs::path testDir;
    std::string dbPath;
    std::string midiDir;
};

// ============================================================================
// TESTS CONSTRUCTION / DESTRUCTION
// ============================================================================

TEST_CASE_METHOD(MidiFileManagerTestFixture, "MidiFileManager - Construction", "[MidiFileManager]") {
    SECTION("Construction normale réussit") {
        REQUIRE_NOTHROW(MidiFileManager(midiDir, dbPath));
    }
    
    SECTION("Crée le répertoire MIDI s'il n'existe pas") {
        std::string newDir = (testDir / "new_midi").string();
        
        REQUIRE_FALSE(fs::exists(newDir));
        
        MidiFileManager manager(newDir, dbPath);
        
        REQUIRE(fs::exists(newDir));
    }
    
    SECTION("Accesseurs fonctionnent") {
        MidiFileManager manager(midiDir, dbPath);
        
        REQUIRE(manager.getRootDirectory() == midiDir);
        REQUIRE(manager.getDatabasePath() == dbPath);
    }
}

// ============================================================================
// TESTS SCAN
// ============================================================================

TEST_CASE_METHOD(MidiFileManagerTestFixture, "MidiFileManager - Scan", "[MidiFileManager][scan]") {
    SECTION("Scan initial trouve les fichiers") {
        // Créer quelques fichiers MIDI
        createDummyMidiFile("test1.mid");
        createDummyMidiFile("test2.mid");
        createDummyMidiFile("test3.mid");
        
        MidiFileManager manager(midiDir, dbPath);
        
        // Callback pour vérifier les résultats
        bool scanComplete = false;
        size_t found = 0;
        size_t added = 0;
        
        manager.setOnScanComplete([&](size_t f, size_t a, size_t u) {
            found = f;
            added = a;
            scanComplete = true;
        });
        
        // Lancer le scan
        REQUIRE(manager.scanLibrary(true, false));
        
        // Attendre la fin du scan
        int timeout = 50; // 5 secondes max
        while (!scanComplete && timeout-- > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        REQUIRE(scanComplete);
        REQUIRE(found == 3);
        REQUIRE(added == 3);
    }
    
    SECTION("Ne peut pas lancer deux scans simultanés") {
        createDummyMidiFile("test.mid");
        
        MidiFileManager manager(midiDir, dbPath);
        
        REQUIRE(manager.scanLibrary(true, false));
        REQUIRE_FALSE(manager.scanLibrary(true, false)); // Deuxième scan échoue
    }
    
    SECTION("isScanning retourne l'état correct") {
        createDummyMidiFile("test.mid");
        
        MidiFileManager manager(midiDir, dbPath);
        
        REQUIRE_FALSE(manager.isScanning());
        
        manager.scanLibrary(true, false);
        
        REQUIRE(manager.isScanning());
        
        // Attendre la fin
        while (manager.isScanning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        REQUIRE_FALSE(manager.isScanning());
    }
}

// ============================================================================
// TESTS RECHERCHE
// ============================================================================

TEST_CASE_METHOD(MidiFileManagerTestFixture, "MidiFileManager - Recherche", "[MidiFileManager][search]") {
    // Créer des fichiers et scanner
    createDummyMidiFile("song_a.mid");
    createDummyMidiFile("song_b.mid");
    createDummyMidiFile("test.mid");
    
    MidiFileManager manager(midiDir, dbPath);
    
    bool scanComplete = false;
    manager.setOnScanComplete([&](size_t, size_t, size_t) {
        scanComplete = true;
    });
    manager.scanLibrary(true, false);
    
    while (!scanComplete) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    SECTION("listFiles retourne tous les fichiers") {
        auto files = manager.listFiles(0, 0);
        
        REQUIRE(files.size() == 3);
    }
    
    SECTION("listFiles avec limite fonctionne") {
        auto files = manager.listFiles(2, 0);
        
        REQUIRE(files.size() == 2);
    }
    
    SECTION("listFiles avec offset fonctionne") {
        auto files = manager.listFiles(10, 2);
        
        REQUIRE(files.size() == 1);
    }
    
    SECTION("searchFiles trouve les fichiers") {
        auto results = manager.searchFiles("song");
        
        REQUIRE(results.size() == 2);
    }
    
    SECTION("searchFiles est insensible à la casse") {
        auto results = manager.searchFiles("SONG");
        
        REQUIRE(results.size() == 2);
    }
    
    SECTION("searchFiles avec query trop courte retourne vide") {
        auto results = manager.searchFiles("a");
        
        REQUIRE(results.empty());
    }
}

// ============================================================================
// TESTS GETFILE
// ============================================================================

TEST_CASE_METHOD(MidiFileManagerTestFixture, "MidiFileManager - GetFile", "[MidiFileManager]") {
    createDummyMidiFile("test.mid");
    
    MidiFileManager manager(midiDir, dbPath);
    
    bool scanComplete = false;
    manager.setOnScanComplete([&](size_t, size_t, size_t) {
        scanComplete = true;
    });
    manager.scanLibrary(true, false);
    
    while (!scanComplete) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    SECTION("getFile avec ID valide retourne le fichier") {
        auto files = manager.listFiles(1, 0);
        REQUIRE_FALSE(files.empty());
        
        std::string id = files[0].id;
        
        auto file = manager.getFile(id);
        
        REQUIRE(file.has_value());
        REQUIRE(file->id == id);
        REQUIRE(file->filename == "test.mid");
    }
    
    SECTION("getFile avec ID invalide retourne nullopt") {
        auto file = manager.getFile("invalid-id");
        
        REQUIRE_FALSE(file.has_value());
    }
}

// ============================================================================
// TESTS PLAYLISTS
// ============================================================================

TEST_CASE_METHOD(MidiFileManagerTestFixture, "MidiFileManager - Playlists", "[MidiFileManager][playlists]") {
    createDummyMidiFile("song1.mid");
    createDummyMidiFile("song2.mid");
    
    MidiFileManager manager(midiDir, dbPath);
    
    bool scanComplete = false;
    manager.setOnScanComplete([&](size_t, size_t, size_t) {
        scanComplete = true;
    });
    manager.scanLibrary(true, false);
    
    while (!scanComplete) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    SECTION("Créer une playlist vide") {
        auto playlist = manager.createPlaylist("Test Playlist", "Description");
        
        REQUIRE_FALSE(playlist.id.empty());
        REQUIRE(playlist.name == "Test Playlist");
        REQUIRE(playlist.description == "Description");
        REQUIRE(playlist.fileIds.empty());
    }
    
    SECTION("Créer une playlist avec fichiers") {
        auto files = manager.listFiles(2, 0);
        std::vector<std::string> ids = {files[0].id, files[1].id};
        
        auto playlist = manager.createPlaylist("My Playlist", "", ids);
        
        REQUIRE(playlist.fileIds.size() == 2);
    }
    
    SECTION("Lister les playlists") {
        manager.createPlaylist("Playlist 1");
        manager.createPlaylist("Playlist 2");
        
        auto playlists = manager.listPlaylists();
        
        REQUIRE(playlists.size() == 2);
    }
    
    SECTION("Récupérer une playlist par ID") {
        auto created = manager.createPlaylist("Test");
        
        auto retrieved = manager.getPlaylist(created.id);
        
        REQUIRE(retrieved.has_value());
        REQUIRE(retrieved->id == created.id);
        REQUIRE(retrieved->name == "Test");
    }
    
    SECTION("Ajouter des fichiers à une playlist") {
        auto playlist = manager.createPlaylist("Test");
        auto files = manager.listFiles(2, 0);
        
        std::vector<std::string> ids = {files[0].id, files[1].id};
        
        REQUIRE(manager.addToPlaylist(playlist.id, ids));
        
        auto updated = manager.getPlaylist(playlist.id);
        REQUIRE(updated->fileIds.size() == 2);
    }
    
    SECTION("Retirer des fichiers d'une playlist") {
        auto files = manager.listFiles(2, 0);
        std::vector<std::string> ids = {files[0].id, files[1].id};
        
        auto playlist = manager.createPlaylist("Test", "", ids);
        
        REQUIRE(manager.removeFromPlaylist(playlist.id, {files[0].id}));
        
        auto updated = manager.getPlaylist(playlist.id);
        REQUIRE(updated->fileIds.size() == 1);
    }
    
    SECTION("Supprimer une playlist") {
        auto playlist = manager.createPlaylist("Test");
        
        REQUIRE(manager.deletePlaylist(playlist.id));
        
        auto retrieved = manager.getPlaylist(playlist.id);
        REQUIRE_FALSE(retrieved.has_value());
    }
    
    SECTION("Renommer une playlist") {
        auto playlist = manager.createPlaylist("Old Name");
        
        REQUIRE(manager.renamePlaylist(playlist.id, "New Name"));
        
        auto updated = manager.getPlaylist(playlist.id);
        REQUIRE(updated->name == "New Name");
    }
}

// ============================================================================
// TESTS MÉTADONNÉES
// ============================================================================

TEST_CASE_METHOD(MidiFileManagerTestFixture, "MidiFileManager - Métadonnées", "[MidiFileManager][metadata]") {
    createDummyMidiFile("test.mid");
    
    MidiFileManager manager(midiDir, dbPath);
    
    bool scanComplete = false;
    manager.setOnScanComplete([&](size_t, size_t, size_t) {
        scanComplete = true;
    });
    manager.scanLibrary(true, false);
    
    while (!scanComplete) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    auto files = manager.listFiles(1, 0);
    REQUIRE_FALSE(files.empty());
    std::string fileId = files[0].id;
    
    SECTION("Mettre à jour les tags") {
        std::vector<std::string> tags = {"rock", "classical", "favorite"};
        
        REQUIRE(manager.updateTags(fileId, tags));
        
        auto file = manager.getFile(fileId);
        REQUIRE(file->tags == tags);
    }
    
    SECTION("Incrémenter playCount") {
        auto before = manager.getFile(fileId);
        int countBefore = before->playCount;
        
        manager.incrementPlayCount(fileId);
        
        auto after = manager.getFile(fileId);
        REQUIRE(after->playCount == countBefore + 1);
    }
}

// ============================================================================
// TESTS STATISTIQUES
// ============================================================================

TEST_CASE_METHOD(MidiFileManagerTestFixture, "MidiFileManager - Statistiques", "[MidiFileManager][stats]") {
    createDummyMidiFile("test1.mid");
    createDummyMidiFile("test2.mid");
    
    MidiFileManager manager(midiDir, dbPath);
    
    bool scanComplete = false;
    manager.setOnScanComplete([&](size_t, size_t, size_t) {
        scanComplete = true;
    });
    manager.scanLibrary(true, false);
    
    while (!scanComplete) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    SECTION("getStatistics retourne les bonnes données") {
        auto stats = manager.getStatistics();
        
        REQUIRE(stats.contains("total_files"));
        REQUIRE(stats["total_files"] == 2);
        
        REQUIRE(stats.contains("total_size_bytes"));
        REQUIRE(stats.contains("total_duration_ms"));
        REQUIRE(stats.contains("total_playlists"));
    }
    
    SECTION("Statistiques incluent les playlists") {
        manager.createPlaylist("Playlist 1");
        manager.createPlaylist("Playlist 2");
        
        auto stats = manager.getStatistics();
        
        REQUIRE(stats["total_playlists"] == 2);
    }
}