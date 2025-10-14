// ============================================================================
// Fichier: tests/test_sysex_critical.cpp
// Tests Unitaires Critiques - SysEx & Validation
// Date: 06/10/2025
// ============================================================================

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include "../src/midi/sysex/SysExHandler.h"
#include "../src/midi/sysex/SysExParser.h"
#include "../src/midi/sysex/SysExBuilder.h"
#include "../src/midi/sysex/CustomSysExParser.h"
#include "../src/midi/MidiRouter.h"
#include "../src/core/Logger.h"

using namespace midiMind;

// ============================================================================
// TEST FIXTURE
// ============================================================================

class SysExCriticalTest : public ::testing::Test {
protected:
    void SetUp() override {
        Logger::instance().setLevel(LogLevel::ERROR);
        handler = std::make_shared<SysExHandler>();
        router = std::make_shared<MidiRouter>();
        router->setSysExHandler(handler);
    }
    
    void TearDown() override {
        handler.reset();
        router.reset();
    }
    
    // Helper: Créer Identity Reply message
    std::vector<uint8_t> createIdentityReplyMessage() {
        // F0 7E 10 06 02 41 19 00 06 00 01 00 00 00 F7
        return {
            0xF0, 0x7E, 0x10, 0x06, 0x02,  // SOX, Non-RT, Device 16, Gen Info, ID Reply
            0x41,                           // Roland
            0x19, 0x00,                     // Family
            0x06, 0x00,                     // Model
            0x01, 0x00, 0x00, 0x00,        // Version
            0xF7                            // EOX
        };
    }
    
    // Helper: Créer Custom Device Identity (Bloc 1)
    std::vector<uint8_t> createCustomIdentityMessage() {
        std::vector<uint8_t> msg = {
            0xF0, 0x7D, 0x00, 0x01, 0x01,  // SOX, Educational, Device 0, Block 1, v1
            0x67, 0x4C, 0x11, 0x09,        // Unique ID (28-bit encoded)
        };
        
        // Name: "TestFlute" (null-terminated)
        std::string name = "TestFlute";
        for (char c : name) {
            msg.push_back(static_cast<uint8_t>(c));
        }
        msg.push_back(0x00);  // Null terminator
        
        msg.push_back(0x80);               // Type: Wind DIY
        msg.push_back(60);                 // First Note: C4
        msg.push_back(24);                 // Note Count: 24
        msg.push_back(0);                  // Mono
        msg.push_back(0);                  // Chromatic
        msg.push_back(30);                 // Delay LSB
        msg.push_back(0);                  // Delay MSB
        msg.push_back(1);                  // FW Major
        msg.push_back(2);                  // FW Minor
        msg.push_back(0);                  // FW Patch
        msg.push_back(10);                 // FW Build
        msg.push_back(0x05);               // Flags: Velocity + Breath
        msg.push_back(4);                  // Programs: 4
        
        msg.push_back(0xF7);               // EOX
        
        return msg;
    }
    
    // Helper: Créer Note Map (Bloc 2)
    std::vector<uint8_t> createNoteMapMessage() {
        std::vector<uint8_t> msg = {
            0xF0, 0x7D, 0x00, 0x02, 0x01,  // SOX, Educational, Device 0, Block 2, v1
        };
        
        // Bitmap: Notes 60-83 jouables (19 bytes)
        for (int i = 0; i < 19; i++) {
            if (i >= 8 && i < 12) {
                msg.push_back(0x7F);  // Notes 60-83
            } else {
                msg.push_back(0x00);
            }
        }
        
        msg.push_back(0x00);  // Reserved
        msg.push_back(0x00);  // Reserved
        msg.push_back(0xF7);  // EOX
        
        return msg;
    }
    
    // Helper: Créer CC Capabilities (Bloc 3)
    std::vector<uint8_t> createCCCapabilitiesMessage() {
        std::vector<uint8_t> msg = {
            0xF0, 0x7D, 0x00, 0x03, 0x02,  // SOX, Educational, Device 0, Block 3, v2
            0x04,                           // 4 CC supportés
            0x01, 0x02, 0x07, 0x0B,        // Mod, Breath, Vol, Expr
            0xF7                            // EOX
        };
        
        return msg;
    }
    
    std::shared_ptr<SysExHandler> handler;
    std::shared_ptr<MidiRouter> router;
};

// ============================================================================
// TEST 1: LOCK ORDERING - PAS DE DEADLOCK
// ============================================================================

TEST_F(SysExCriticalTest, NoDeadlockOnCallback) {
    bool callbackExecuted = false;
    bool cacheAccessSuccessful = false;
    
    // Configure callback qui accède au cache
    handler->setOnDeviceIdentified(
        [this, &callbackExecuted, &cacheAccessSuccessful](
            const std::string& deviceId, 
            const DeviceIdentity& identity
        ) {
            callbackExecuted = true;
            
            // Ce call NE DOIT PAS deadlock
            auto cached = handler->getDeviceIdentity(deviceId);
            
            if (cached.has_value()) {
                cacheAccessSuccessful = true;
            }
        }
    );
    
    // Simuler Identity Reply
    auto msg = createIdentityReplyMessage();
    handler->handleSysExMessage(msg, "test_device");
    
    // Vérifications
    ASSERT_TRUE(callbackExecuted) << "Callback n'a pas été exécuté";
    ASSERT_TRUE(cacheAccessSuccessful) << "Accès cache a échoué (deadlock?)";
}

// ============================================================================
// TEST 2: CALLBACKS TOUS DÉCLENCHÉS
// ============================================================================

TEST_F(SysExCriticalTest, AllCallbacksTriggered) {
    bool deviceIdentifiedCalled = false;
    bool customIdentifiedCalled = false;
    bool noteMapCalled = false;
    bool ccCapsCalled = false;
    
    // Configure tous les callbacks
    handler->setOnDeviceIdentified(
        [&](const std::string&, const DeviceIdentity&) {
            deviceIdentifiedCalled = true;
        }
    );
    
    handler->setOnCustomDeviceIdentified(
        [&](const std::string&, const CustomDeviceIdentity&) {
            customIdentifiedCalled = true;
        }
    );
    
    handler->setOnNoteMapReceived(
        [&](const std::string&, const NoteMap&) {
            noteMapCalled = true;
        }
    );
    
    handler->setOnCCCapabilities(
        [&](const std::string&, const CCCapabilities&) {
            ccCapsCalled = true;
        }
    );
    
    // Envoyer les messages
    handler->handleSysExMessage(createIdentityReplyMessage(), "device1");
    handler->handleSysExMessage(createCustomIdentityMessage(), "device1");
    handler->handleSysExMessage(createNoteMapMessage(), "device1");
    handler->handleSysExMessage(createCCCapabilitiesMessage(), "device1");
    
    // Vérifier tous appelés
    ASSERT_TRUE(deviceIdentifiedCalled) << "Device Identified callback non appelé";
    ASSERT_TRUE(customIdentifiedCalled) << "Custom Identified callback non appelé";
    ASSERT_TRUE(noteMapCalled) << "Note Map callback non appelé";
    ASSERT_TRUE(ccCapsCalled) << "CC Capabilities callback non appelé";
}

// ============================================================================
// TEST 3: CACHE THREAD-SAFE
// ============================================================================

TEST_F(SysExCriticalTest, CacheThreadSafe) {
    const int NUM_THREADS = 10;
    const int ITERATIONS = 100;
    
    std::atomic<int> successCount{0};
    std::vector<std::thread> threads;
    
    // Pré-remplir le cache
    handler->handleSysExMessage(createCustomIdentityMessage(), "device1");
    handler->handleSysExMessage(createNoteMapMessage(), "device1");
    
    // Lancer plusieurs threads qui lisent le cache
    for (int t = 0; t < NUM_THREADS; t++) {
        threads.emplace_back([this, &successCount]() {
            for (int i = 0; i < ITERATIONS; i++) {
                auto identity = handler->getCustomIdentity("device1");
                auto noteMap = handler->getNoteMap("device1");
                
                if (identity.has_value() && noteMap.has_value()) {
                    successCount++;
                }
            }
        });
    }
    
    // Attendre tous les threads
    for (auto& t : threads) {
        t.join();
    }
    
    // Vérifier aucune corruption
    ASSERT_EQ(successCount.load(), NUM_THREADS * ITERATIONS) 
        << "Race condition détectée dans le cache";
}

// ============================================================================
// TEST 4: VALIDATION NOTES JOUABLES
// ============================================================================

TEST_F(SysExCriticalTest, ValidationNotePlayable) {
    // Configurer Note Map (notes 60-83 jouables)
    handler->handleSysExMessage(createNoteMapMessage(), "device1");
    
    // Attendre que le message soit traité
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Créer messages MIDI
    MidiMessage validNote({0x90, 64, 100});    // Note On, note 64 (jouable)
    MidiMessage invalidNote({0x90, 48, 100});  // Note On, note 48 (non jouable)
    
    // Tester validation
    bool validResult = router->validateMessage(validNote, "device1");
    bool invalidResult = router->validateMessage(invalidNote, "device1");
    
    ASSERT_TRUE(validResult) << "Note 64 devrait être valide";
    ASSERT_FALSE(invalidResult) << "Note 48 devrait être bloquée";
}

// ============================================================================
// TEST 5: VALIDATION CC SUPPORTÉS
// ============================================================================

TEST_F(SysExCriticalTest, ValidationCCSupported) {
    // Configurer CC Capabilities (CC 1, 2, 7, 11)
    handler->handleSysExMessage(createCCCapabilitiesMessage(), "device1");
    
    // Attendre traitement
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Créer messages CC
    MidiMessage validCC({0xB0, 0x01, 64});    // CC 1 (Modulation) - supporté
    MidiMessage invalidCC({0xB0, 0x4A, 64});  // CC 74 (Brightness) - non supporté
    
    // Tester validation
    bool validResult = router->validateMessage(validCC, "device1");
    bool invalidResult = router->validateMessage(invalidCC, "device1");
    
    ASSERT_TRUE(validResult) << "CC 1 devrait être valide";
    ASSERT_FALSE(invalidResult) << "CC 74 devrait être bloqué";
}

// ============================================================================
// TEST 6: STATISTIQUES VALIDATION
// ============================================================================

TEST_F(SysExCriticalTest, ValidationStatistics) {
    // Configurer capacités
    handler->handleSysExMessage(createNoteMapMessage(), "device1");
    handler->handleSysExMessage(createCCCapabilitiesMessage(), "device1");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Reset stats
    router->resetStatistics();
    
    // Envoyer messages valides et invalides
    MidiMessage validNote({0x90, 64, 100});
    MidiMessage invalidNote({0x90, 48, 100});
    MidiMessage validCC({0xB0, 0x01, 64});
    MidiMessage invalidCC({0xB0, 0x4A, 64});
    
    router->validateMessage(validNote, "device1");    // OK
    router->validateMessage(invalidNote, "device1");  // Bloquée
    router->validateMessage(validCC, "device1");      // OK
    router->validateMessage(invalidCC, "device1");    // Bloqué
    
    // Vérifier statistiques
    json stats = router->getStats();
    
    ASSERT_EQ(stats["messages_validated"].get<int>(), 4);
    ASSERT_EQ(stats["validation"]["notes_blocked"].get<int>(), 1);
    ASSERT_EQ(stats["validation"]["cc_blocked"].get<int>(), 1);
    ASSERT_EQ(stats["validation"]["total_blocked"].get<int>(), 2);
}

// ============================================================================
// TEST 7: CLEAR IDENTITY FONCTIONNE
// ============================================================================

TEST_F(SysExCriticalTest, ClearIdentityWorks) {
    // Remplir le cache
    handler->handleSysExMessage(createCustomIdentityMessage(), "device1");
    handler->handleSysExMessage(createNoteMapMessage(), "device1");
    handler->handleSysExMessage(createCCCapabilitiesMessage(), "device1");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Vérifier présence
    ASSERT_TRUE(handler->getCustomIdentity("device1").has_value());
    ASSERT_TRUE(handler->getNoteMap("device1").has_value());
    ASSERT_TRUE(handler->getCCCapabilities("device1").has_value());
    
    // Clear
    handler->clearCustomIdentity("device1");
    
    // Vérifier absence
    ASSERT_FALSE(handler->getCustomIdentity("device1").has_value());
    ASSERT_FALSE(handler->getNoteMap("device1").has_value());
    ASSERT_FALSE(handler->getCCCapabilities("device1").has_value());
}

// ============================================================================
// TEST 8: PARSING CUSTOM SYSEX
// ============================================================================

TEST_F(SysExCriticalTest, CustomSysExParsing) {
    auto msg = createCustomIdentityMessage();
    SysExMessage sysexMsg(msg);
    
    // Vérifier détection Custom SysEx
    ASSERT_TRUE(CustomSysExParser::isCustomSysEx(sysexMsg));
    
    // Vérifier Block ID
    auto blockId = CustomSysExParser::getBlockId(sysexMsg);
    ASSERT_TRUE(blockId.has_value());
    ASSERT_EQ(blockId.value(), 0x01);
    
    // Parser
    auto identity = CustomSysExParser::parseIdentification(sysexMsg);
    ASSERT_TRUE(identity.has_value());
    ASSERT_EQ(identity->name, "TestFlute");
    ASSERT_EQ(identity->type, 0x80);
    ASSERT_EQ(identity->firstNote, 60);
    ASSERT_EQ(identity->noteCount, 24);
}

// ============================================================================
// TEST 9: AUTO-IDENTIFY CONFIGURATION
// ============================================================================

TEST_F(SysExCriticalTest, AutoIdentifyConfiguration) {
    // Vérifier valeurs par défaut
    ASSERT_TRUE(handler->isAutoIdentifyEnabled());
    ASSERT_EQ(handler->getAutoIdentifyDelay(), 500);
    
    // Modifier
    handler->setAutoIdentify(false);
    handler->setAutoIdentifyDelay(1000);
    
    // Vérifier modifications
    ASSERT_FALSE(handler->isAutoIdentifyEnabled());
    ASSERT_EQ(handler->getAutoIdentifyDelay(), 1000);
}

// ============================================================================
// TEST 10: MULTI-DEVICES
// ============================================================================

TEST_F(SysExCriticalTest, MultipleDevices) {
    // Configurer 3 devices différents
    handler->handleSysExMessage(createCustomIdentityMessage(), "device1");
    handler->handleSysExMessage(createCustomIdentityMessage(), "device2");
    handler->handleSysExMessage(createCustomIdentityMessage(), "device3");
    
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    
    // Vérifier séparation des caches
    auto id1 = handler->getCustomIdentity("device1");
    auto id2 = handler->getCustomIdentity("device2");
    auto id3 = handler->getCustomIdentity("device3");
    
    ASSERT_TRUE(id1.has_value());
    ASSERT_TRUE(id2.has_value());
    ASSERT_TRUE(id3.has_value());
    
    // Clear device2 uniquement
    handler->clearCustomIdentity("device2");
    
    // Vérifier device1 et device3 toujours présents
    ASSERT_TRUE(handler->getCustomIdentity("device1").has_value());
    ASSERT_FALSE(handler->getCustomIdentity("device2").has_value());
    ASSERT_TRUE(handler->getCustomIdentity("device3").has_value());
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

// ============================================================================
// FIN DU FICHIER test_sysex_critical.cpp
// ============================================================================
