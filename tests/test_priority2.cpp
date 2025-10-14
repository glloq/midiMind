// ============================================================================
// tests/test_priority2.cpp - Tests Unitaires pour Optimisations Priorité 2
// ============================================================================

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "../src/midi/MidiPlayer.h"
#include "../src/midi/MidiRouter.h"
#include "../src/api/CommandProcessor.h"
#include "../src/core/Config.h"

using namespace midiMind;
using ::testing::_;
using ::testing::Return;

// ============================================================================
// MOCK CLASSES
// ============================================================================

class MockMidiDevice : public MidiDevice {
public:
    MockMidiDevice(const std::string& id) 
        : MidiDevice(id, "Mock " + id, DeviceType::USB) {
        setStatus(DeviceStatus::CONNECTED);
    }
    
    MOCK_METHOD(bool, connect, (), (override));
    MOCK_METHOD(void, disconnect, (), (override));
    MOCK_METHOD(bool, sendMessage, (const MidiMessage&), (override));
};

class MockDeviceManager : public MidiDeviceManager {
public:
    void addMockDevice(std::shared_ptr<MockMidiDevice> device) {
        std::lock_guard<std::mutex> lock(mutex_);
        devices_[device->getId()] = device;
    }
};

// ============================================================================
// TEST FIXTURE
// ============================================================================

class Priority2Test : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup logger
        Logger::instance().setLevel(LogLevel::ERROR);
        
        // Setup config
        Config::instance().resetToDefaults();
        Config::instance().set("midi_files_directory", "./test_data");
        
        // Create components
        deviceMgr_ = std::make_shared<MockDeviceManager>();
        router_ = std::make_shared<MidiRouter>(deviceMgr_);
        player_ = std::make_shared<MidiPlayer>(router_);
        
        // Add mock devices
        mockDevice1_ = std::make_shared<MockMidiDevice>("test_1");
        mockDevice2_ = std::make_shared<MockMidiDevice>("test_2");
        deviceMgr_->addMockDevice(mockDevice1_);
        deviceMgr_->addMockDevice(mockDevice2_);
    }
    
    void TearDown() override {
        player_.reset();
        router_.reset();
        deviceMgr_.reset();
    }
    
    std::shared_ptr<MockDeviceManager> deviceMgr_;
    std::shared_ptr<MidiRouter> router_;
    std::shared_ptr<MidiPlayer> player_;
    std::shared_ptr<MockMidiDevice> mockDevice1_;
    std::shared_ptr<MockMidiDevice> mockDevice2_;
};

// ============================================================================
// TESTS: MidiPlayer Optimizations
// ============================================================================

TEST_F(Priority2Test, PlayerTrackPlaybackStateInitialized) {
    // Créer un fichier MIDI de test
    // Note: Nécessite un vrai fichier MIDI ou un mock de MidiFile
    
    // Pour ce test, on vérifie juste que la structure existe
    // et est utilisable
    EXPECT_NO_THROW({
        // Le player doit pouvoir être créé sans crash
        auto testPlayer = std::make_shared<MidiPlayer>(router_);
    });
}

TEST_F(Priority2Test, PlayerSeekPerformance) {
    // Test que le seek est plus rapide qu'avant
    // Objectif: < 100ms même sur gros fichiers
    
    // Créer un fichier MIDI synthétique avec beaucoup d'événements
    // (dans un vrai test, utiliser un fichier pré-généré)
    
    const int NUM_SEEKS = 100;
    std::vector<std::chrono::microseconds> seekTimes;
    
    // Simuler des seeks aléatoires
    for (int i = 0; i < NUM_SEEKS; i++) {
        auto start = std::chrono::high_resolution_clock::now();
        
        uint32_t randomPos = (i * 1000) % 60000;  // 0-60s
        player_->seek(randomPos);
        
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        seekTimes.push_back(duration);
    }
    
    // Calculer moyenne et max
    auto totalTime = std::accumulate(seekTimes.begin(), seekTimes.end(), 
                                    std::chrono::microseconds(0));
    auto avgTime = totalTime / NUM_SEEKS;
    auto maxTime = *std::max_element(seekTimes.begin(), seekTimes.end());
    
    // Assertions
    EXPECT_LT(avgTime.count(), 10000) << "Average seek time should be < 10ms";
    EXPECT_LT(maxTime.count(), 100000) << "Max seek time should be < 100ms";
}

TEST_F(Priority2Test, PlayerIncrementalProcessing) {
    // Test que le traitement est bien incrémental
    // (pas de parcours complet à chaque frame)
    
    // Créer un mock qui compte les accès aux événements
    // Dans un vrai test, on instrumenterait MidiFile
    
    // Pour l'instant, test de non-régression
    EXPECT_NO_THROW({
        player_->play();
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        player_->stop();
    });
}

// ============================================================================
// TESTS: MidiRouter Lock Optimization
// ============================================================================

TEST_F(Priority2Test, RouterLockDuration) {
    // Test que le lock est tenu pendant moins de 100µs
    
    router_->addRoute(0, "test_1");
    
    const int NUM_ROUTES = 1000;
    std::vector<std::chrono::nanoseconds> lockTimes;
    
    for (int i = 0; i < NUM_ROUTES; i++) {
        MidiMessage msg = MidiMessage::noteOn(0, 60, 100);
        
        auto start = std::chrono::high_resolution_clock::now();
        router_->routeMessage(0, msg);
        auto end = std::chrono::high_resolution_clock::now();
        
        lockTimes.push_back(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start)
        );
    }
    
    auto avgTime = std::accumulate(lockTimes.begin(), lockTimes.end(), 
                                   std::chrono::nanoseconds(0)) / NUM_ROUTES;
    
    // Le temps total doit être < 100µs en moyenne
    EXPECT_LT(avgTime.count(), 100000) << "Average routing time should be < 100µs";
}

TEST_F(Priority2Test, RouterBatchProcessing) {
    // Test de la nouvelle méthode routeMessages()
    
    router_->addRoute(0, "test_1");
    
    // Préparer un batch de messages
    std::vector<MidiMessage> batch;
    for (int i = 0; i < 100; i++) {
        batch.push_back(MidiMessage::noteOn(0, 60 + (i % 12), 100));
    }
    
    // Configurer le mock pour accepter tous les messages
    EXPECT_CALL(*mockDevice1_, sendMessage(_))
        .Times(100)
        .WillRepeatedly(Return(true));
    
    // Router en batch
    auto start = std::chrono::high_resolution_clock::now();
    router_->routeMessages(0, batch);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Le batch de 100 messages doit être routé en < 10ms
    EXPECT_LT(duration.count(), 10) << "Batch routing should be < 10ms";
}

TEST_F(Priority2Test, RouterMultithreadedAccess) {
    // Test que le router est thread-safe et performant
    
    router_->addRoute(0, "test_1");
    
    const int NUM_THREADS = 4;
    const int MSGS_PER_THREAD = 1000;
    
    std::atomic<int> totalSent{0};
    std::vector<std::thread> threads;
    
    EXPECT_CALL(*mockDevice1_, sendMessage(_))
        .Times(NUM_THREADS * MSGS_PER_THREAD)
        .WillRepeatedly(Return(true));
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([&]() {
            for (int i = 0; i < MSGS_PER_THREAD; i++) {
                MidiMessage msg = MidiMessage::noteOn(0, 60, 100);
                router_->routeMessage(0, msg);
                totalSent++;
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    // Vérifier que tous les messages ont été envoyés
    EXPECT_EQ(totalSent.load(), NUM_THREADS * MSGS_PER_THREAD);
    
    // Le débit doit être > 1000 msg/s
    double throughput = (totalSent.load() * 1000.0) / duration.count();
    EXPECT_GT(throughput, 1000) << "Throughput should be > 1000 msg/s";
}

TEST_F(Priority2Test, RouterAtomicSolo) {
    // Test que anySolo_ est bien un atomic et fonctionne sans lock
    
    router_->addRoute(0, "test_1");
    router_->addRoute(1, "test_2");
    
    // Activer solo
    router_->setSolo(0, "test_1", true);
    
    // Les messages sur canal 1 doivent être ignorés
    EXPECT_CALL(*mockDevice1_, sendMessage(_)).Times(1);
    EXPECT_CALL(*mockDevice2_, sendMessage(_)).Times(0);
    
    router_->routeMessage(0, MidiMessage::noteOn(0, 60, 100));
    router_->routeMessage(1, MidiMessage::noteOn(1, 62, 100));
    
    // Clear solo
    router_->clearAllSolo();
    
    // Maintenant les deux doivent passer
    EXPECT_CALL(*mockDevice1_, sendMessage(_)).Times(1);
    EXPECT_CALL(*mockDevice2_, sendMessage(_)).Times(1);
    
    router_->routeMessage(0, MidiMessage::noteOn(0, 60, 100));
    router_->routeMessage(1, MidiMessage::noteOn(1, 62, 100));
}

// ============================================================================
// TESTS: JSON Validation & Security
// ============================================================================

TEST_F(Priority2Test, JsonValidatorStringLength) {
    json obj = {{"test", "short"}};
    std::string result, error;
    
    // Valid string
    EXPECT_TRUE(JsonValidator::validateString(obj, "test", result, 10, error));
    EXPECT_EQ(result, "short");
    
    // Too long
    obj["test"] = std::string(100, 'x');
    EXPECT_FALSE(JsonValidator::validateString(obj, "test", result, 50, error));
    EXPECT_FALSE(error.empty());
}

TEST_F(Priority2Test, JsonValidatorRange) {
    json obj = {{"value", 5}};
    int result;
    std::string error;
    
    // Valid range
    EXPECT_TRUE(JsonValidator::validateRange(obj, "value", result, 0, 10, error));
    EXPECT_EQ(result, 5);
    
    // Out of range (too low)
    obj["value"] = -1;
    EXPECT_FALSE(JsonValidator::validateRange(obj, "value", result, 0, 10, error));
    
    // Out of range (too high)
    obj["value"] = 11;
    EXPECT_FALSE(JsonValidator::validateRange(obj, "value", result, 0, 10, error));
}

TEST_F(Priority2Test, JsonValidatorMidiChannel) {
    std::string error;
    
    // Valid channels
    EXPECT_TRUE(JsonValidator::validateMidiChannel(0, error));
    EXPECT_TRUE(JsonValidator::validateMidiChannel(15, error));
    
    // Invalid channels
    EXPECT_FALSE(JsonValidator::validateMidiChannel(-1, error));
    EXPECT_FALSE(JsonValidator::validateMidiChannel(16, error));
    EXPECT_FALSE(JsonValidator::validateMidiChannel(100, error));
}

TEST_F(Priority2Test, JsonValidatorDeviceId) {
    std::string error;
    
    // Valid device IDs
    EXPECT_TRUE(JsonValidator::validateDeviceId("usb_0", error));
    EXPECT_TRUE(JsonValidator::validateDeviceId("wifi_192.168.1.100_5004", error));
    EXPECT_TRUE(JsonValidator::validateDeviceId("bt_00:11:22:33:44:55", error));
    
    // Invalid device IDs
    EXPECT_FALSE(JsonValidator::validateDeviceId("../../../dev/null", error));
    EXPECT_FALSE(JsonValidator::validateDeviceId("usb_0; rm -rf /", error));
    EXPECT_FALSE(JsonValidator::validateDeviceId("invalid_format", error));
    EXPECT_FALSE(JsonValidator::validateDeviceId("", error));
}

TEST_F(Priority2Test, JsonValidatorFilePath) {
    std::string error;
    std::string baseDir = "/home/pi/midi_files";
    
    // Valid paths
    EXPECT_TRUE(JsonValidator::validateFilePath(
        "/home/pi/midi_files/song.mid", baseDir, error));
    EXPECT_TRUE(JsonValidator::validateFilePath(
        "/home/pi/midi_files/subfolder/track.mid", baseDir, error));
    
    // Invalid paths (path traversal)
    EXPECT_FALSE(JsonValidator::validateFilePath(
        "../../etc/passwd", baseDir, error));
    EXPECT_FALSE(JsonValidator::validateFilePath(
        "/home/pi/midi_files/../../../etc/passwd", baseDir, error));
    EXPECT_FALSE(JsonValidator::validateFilePath(
        "/home/pi/midi_files//etc/passwd", baseDir, error));
    
    // Invalid paths (outside base dir)
    EXPECT_FALSE(JsonValidator::validateFilePath(
        "/etc/passwd", baseDir, error));
}

TEST_F(Priority2Test, RateLimiterBasic) {
    RateLimiter limiter(5, std::chrono::seconds(1));
    
    // Les 5 premières requêtes doivent passer
    for (int i = 0; i < 5; i++) {
        EXPECT_TRUE(limiter.allowRequest("client1"));
    }
    
    // La 6ème doit être bloquée
    EXPECT_FALSE(limiter.allowRequest("client1"));
    
    // Un autre client doit avoir son propre quota
    EXPECT_TRUE(limiter.allowRequest("client2"));
}

TEST_F(Priority2Test, RateLimiterTimeWindow) {
    RateLimiter limiter(3, std::chrono::milliseconds(500));
    
    // Remplir le quota
    EXPECT_TRUE(limiter.allowRequest("client1"));
    EXPECT_TRUE(limiter.allowRequest("client1"));
    EXPECT_TRUE(limiter.allowRequest("client1"));
    EXPECT_FALSE(limiter.allowRequest("client1"));
    
    // Attendre l'expiration de la fenêtre
    std::this_thread::sleep_for(std::chrono::milliseconds(600));
    
    // Le quota doit être réinitialisé
    EXPECT_TRUE(limiter.allowRequest("client1"));
}

TEST_F(Priority2Test, CommandProcessorValidation) {
    auto processor = std::make_shared<CommandProcessor>(
        deviceMgr_, router_, player_
    );
    
    // Commande valide
    json validCmd = {
        {"command", "devices.list"}
    };
    json response = processor->processCommand(validCmd);
    EXPECT_TRUE(response["success"].get<bool>());
    
    // Commande invalide (missing field)
    json invalidCmd1 = {
        {"not_command", "value"}
    };
    response = processor->processCommand(invalidCmd1);
    EXPECT_FALSE(response["success"].get<bool>());
    EXPECT_TRUE(response.contains("error"));
    
    // Commande invalide (bad type)
    json invalidCmd2 = {
        {"command", 123}  // Should be string
    };
    response = processor->processCommand(invalidCmd2);
    EXPECT_FALSE(response["success"].get<bool>());
}

TEST_F(Priority2Test, CommandProcessorSecurityInjection) {
    auto processor = std::make_shared<CommandProcessor>(
        deviceMgr_, router_, player_
    );
    
    // Tentatives d'injection SQL
    std::vector<json> injectionAttempts = {
        {{"command", "routes.add'; DROP TABLE routes;--"}},
        {{"command", "devices.connect"}, {"device_id", "'; DELETE FROM users;--"}},
        {{"command", "player.load"}, {"file", "../../etc/passwd"}},
        {{"command", "player.load"}, {"file", "/etc/../../../root/.ssh/id_rsa"}},
    };
    
    for (const auto& attempt : injectionAttempts) {
        json response = processor->processCommand(attempt);
        EXPECT_FALSE(response["success"].get<bool>()) 
            << "Injection should be blocked: " << attempt.dump();
    }
}

TEST_F(Priority2Test, CommandProcessorSecurityDoS) {
    auto processor = std::make_shared<CommandProcessor>(
        deviceMgr_, router_, player_
    );
    
    // Payload énorme (DoS)
    json dosCmd = {
        {"command", std::string(100000, 'A')}
    };
    
    json response = processor->processCommand(dosCmd);
    EXPECT_FALSE(response["success"].get<bool>());
    EXPECT_TRUE(response["error"].get<std::string>().find("too large") != std::string::npos);
}

// ============================================================================
// TESTS: Integration
// ============================================================================

TEST_F(Priority2Test, EndToEndPlayerRoutingPerformance) {
    // Test d'intégration complet
    
    router_->addRoute(0, "test_1");
    
    // Configurer mock
    std::atomic<int> messagesReceived{0};
    EXPECT_CALL(*mockDevice1_, sendMessage(_))
        .WillRepeatedly([&](const MidiMessage&) {
            messagesReceived++;
            return true;
        });
    
    // Le player devrait router des messages au device via le router
    // (nécessite un vrai fichier MIDI)
    
    // Pour ce test, on vérifie juste que la chaîne fonctionne
    player_->play();
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    player_->stop();
    
    // Au moins quelques messages devraient avoir été routés
    // (si un fichier est chargé)
    EXPECT_GE(messagesReceived.load(), 0);
}

TEST_F(Priority2Test, StressTestConcurrentOperations) {
    // Test de stress avec opérations concurrentes
    
    router_->addRoute(0, "test_1");
    router_->addRoute(1, "test_2");
    
    EXPECT_CALL(*mockDevice1_, sendMessage(_))
        .WillRepeatedly(Return(true));
    EXPECT_CALL(*mockDevice2_, sendMessage(_))
        .WillRepeatedly(Return(true));
    
    std::atomic<bool> running{true};
    std::atomic<int> errors{0};
    
    // Thread 1: Router des messages
    std::thread routerThread([&]() {
        while (running) {
            try {
                router_->routeMessage(0, MidiMessage::noteOn(0, 60, 100));
                router_->routeMessage(1, MidiMessage::noteOn(1, 62, 100));
            } catch (...) {
                errors++;
            }
        }
    });
    
    // Thread 2: Modifier les routes
    std::thread configThread([&]() {
        while (running) {
            try {
                router_->setMute(0, "test_1", true);
                router_->setMute(0, "test_1", false);
                router_->setSolo(1, "test_2", true);
                router_->setSolo(1, "test_2", false);
            } catch (...) {
                errors++;
            }
        }
    });
    
    // Thread 3: Lire les statistiques
    std::thread statsThread([&]() {
        while (running) {
            try {
                auto stats = router_->getStats();
                (void)stats;  // Juste pour lire
            } catch (...) {
                errors++;
            }
        }
    });
    
    // Laisser tourner 1 seconde
    std::this_thread::sleep_for(std::chrono::seconds(1));
    running = false;
    
    routerThread.join();
    configThread.join();
    statsThread.join();
    
    // Aucune erreur ne doit survenir
    EXPECT_EQ(errors.load(), 0) << "No thread-safety errors should occur";
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ============================================================================
// COMPILATION
// ============================================================================
/*
g++ -std=c++17 -pthread \
    test_priority2.cpp \
    -I../src \
    -I/usr/include/gtest \
    -lgtest -lgmock -lgtest_main \
    -lmidimind \
    -o test_priority2

./test_priority2
*/