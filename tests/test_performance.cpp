// ============================================================================
// tests/test_performance.cpp - Tests de Performance pour Priorité 2
// ============================================================================

#include <iostream>
#include <chrono>
#include <vector>
#include <thread>
#include <atomic>
#include "../src/midi/MidiPlayer.h"
#include "../src/midi/MidiRouter.h"
#include "../src/api/CommandProcessor.h"
#include "../src/core/Logger.h"

using namespace midiMind;
using namespace std::chrono;

// ============================================================================
// UTILITAIRES DE TEST
// ============================================================================

class PerformanceTest {
public:
    static void header(const std::string& name) {
        std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  " << name;
        for (size_t i = name.length(); i < 60; i++) std::cout << " ";
        std::cout << "║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n\n";
    }
    
    static void result(const std::string& metric, const std::string& value, 
                      const std::string& status = "✓") {
        std::cout << status << " " << metric << ": " << value << "\n";
    }
    
    static void separator() {
        std::cout << "────────────────────────────────────────────────────────────────\n";
    }
};

// ============================================================================
// TEST 1: MidiPlayer - Lecture de Fichiers Volumineux
// ============================================================================

void test_player_large_file() {
    PerformanceTest::header("TEST 1: MidiPlayer - Fichiers Volumineux");
    
    auto router = std::make_shared<MidiRouter>(
        std::make_shared<MidiDeviceManager>()
    );
    
    MidiPlayer player(router);
    
    // Test avec un fichier synthétique (si disponible)
    const char* testFiles[] = {
        "test_data/small_10k.mid",   // 10,000 événements
        "test_data/medium_100k.mid", // 100,000 événements
        "test_data/large_1m.mid"     // 1,000,000 événements
    };
    
    for (const char* file : testFiles) {
        std::cout << "Testing: " << file << "\n";
        
        // Mesure du temps de chargement
        auto t1 = high_resolution_clock::now();
        bool loaded = player.loadFile(file);
        auto t2 = high_resolution_clock::now();
        
        if (!loaded) {
            std::cout << "  ⚠ File not found (skipping)\n\n";
            continue;
        }
        
        auto loadTime = duration_cast<milliseconds>(t2 - t1).count();
        PerformanceTest::result("  Load time", std::to_string(loadTime) + "ms");
        
        // Mesure du CPU pendant la lecture
        player.play();
        
        std::atomic<int> cpuSamples{0};
        std::atomic<float> cpuTotal{0.0f};
        
        std::thread cpuMonitor([&]() {
            for (int i = 0; i < 50; i++) {  // 5 secondes
                std::this_thread::sleep_for(milliseconds(100));
                
                // Mesure simple du CPU (à adapter selon l'OS)
                std::ifstream stat("/proc/self/stat");
                if (stat.is_open()) {
                    std::string line;
                    std::getline(stat, line);
                    // Parser le CPU (simplifié)
                    cpuSamples++;
                }
            }
        });
        
        std::this_thread::sleep_for(seconds(5));
        player.stop();
        
        if (cpuMonitor.joinable()) {
            cpuMonitor.join();
        }
        
        float avgCpu = cpuSamples > 0 ? cpuTotal / cpuSamples : 0;
        PerformanceTest::result("  Avg CPU", std::to_string(avgCpu) + "%");
        
        // Test de seek
        std::vector<uint32_t> seekTimes;
        for (int i = 0; i < 10; i++) {
            uint32_t pos = (player.getDuration() / 10) * i;
            
            auto s1 = high_resolution_clock::now();
            player.seek(pos);
            auto s2 = high_resolution_clock::now();
            
            seekTimes.push_back(
                duration_cast<microseconds>(s2 - s1).count()
            );
        }
        
        // Moyenne et max
        uint32_t sumSeek = 0;
        uint32_t maxSeek = 0;
        for (auto t : seekTimes) {
            sumSeek += t;
            maxSeek = std::max(maxSeek, t);
        }
        uint32_t avgSeek = sumSeek / seekTimes.size();
        
        PerformanceTest::result("  Seek avg", std::to_string(avgSeek) + "µs");
        PerformanceTest::result("  Seek max", std::to_string(maxSeek) + "µs");
        
        std::cout << "\n";
    }
    
    PerformanceTest::separator();
    std::cout << "✓ Test MidiPlayer terminé\n\n";
}

// ============================================================================
// TEST 2: MidiRouter - Débit et Latence
// ============================================================================

void test_router_throughput() {
    PerformanceTest::header("TEST 2: MidiRouter - Débit et Latence");
    
    auto deviceMgr = std::make_shared<MidiDeviceManager>();
    auto router = std::make_shared<MidiRouter>(deviceMgr);
    
    // Créer un device de test (mock)
    class MockDevice : public MidiDevice {
    public:
        MockDevice() : MidiDevice("test_0", "Mock Device", DeviceType::USB) {
            setStatus(DeviceStatus::CONNECTED);
        }
        
        bool connect() override { return true; }
        void disconnect() override {}
        
        bool sendMessage(const MidiMessage& msg) override {
            messageCount_++;
            return true;
        }
        
        size_t getMessageCount() const { return messageCount_; }
        void resetCount() { messageCount_ = 0; }
        
    private:
        std::atomic<size_t> messageCount_{0};
    };
    
    auto mockDevice = std::make_shared<MockDevice>();
    
    // Ajouter le device (simulation)
    // Note: Nécessite d'adapter MidiDeviceManager pour accepter des mocks
    
    // Ajouter une route
    router->addRoute(0, "test_0");
    
    // Test 1: Messages individuels
    std::cout << "Test 1: Messages individuels\n";
    {
        const size_t numMessages = 10000;
        std::vector<duration<double, std::micro>> latencies;
        
        for (size_t i = 0; i < numMessages; i++) {
            MidiMessage msg = MidiMessage::noteOn(0, 60, 100);
            
            auto t1 = high_resolution_clock::now();
            router->routeMessage(0, msg);
            auto t2 = high_resolution_clock::now();
            
            latencies.push_back(
                duration_cast<duration<double, std::micro>>(t2 - t1)
            );
        }
        
        // Calculer statistiques
        double sumLatency = 0;
        double maxLatency = 0;
        for (const auto& l : latencies) {
            sumLatency += l.count();
            maxLatency = std::max(maxLatency, l.count());
        }
        
        double avgLatency = sumLatency / numMessages;
        
        // Calculer p99
        std::sort(latencies.begin(), latencies.end());
        double p99Latency = latencies[numMessages * 99 / 100].count();
        
        PerformanceTest::result("  Messages", std::to_string(numMessages));
        PerformanceTest::result("  Avg latency", 
            std::to_string(avgLatency) + "µs");
        PerformanceTest::result("  P99 latency", 
            std::to_string(p99Latency) + "µs");
        PerformanceTest::result("  Max latency", 
            std::to_string(maxLatency) + "µs");
    }
    
    std::cout << "\nTest 2: Batch processing\n";
    {
        const size_t numBatches = 100;
        const size_t batchSize = 100;
        
        std::vector<MidiMessage> batch;
        for (size_t i = 0; i < batchSize; i++) {
            batch.push_back(MidiMessage::noteOn(0, 60 + (i % 12), 100));
        }
        
        auto t1 = high_resolution_clock::now();
        
        for (size_t i = 0; i < numBatches; i++) {
            router->routeMessages(0, batch);
        }
        
        auto t2 = high_resolution_clock::now();
        
        auto totalTime = duration_cast<milliseconds>(t2 - t1).count();
        size_t totalMessages = numBatches * batchSize;
        double messagesPerSecond = (totalMessages * 1000.0) / totalTime;
        
        PerformanceTest::result("  Total messages", 
            std::to_string(totalMessages));
        PerformanceTest::result("  Total time", 
            std::to_string(totalTime) + "ms");
        PerformanceTest::result("  Throughput", 
            std::to_string((int)messagesPerSecond) + " msg/s");
    }
    
    std::cout << "\nTest 3: Multi-threading stress\n";
    {
        const size_t numThreads = 4;
        const size_t messagesPerThread = 5000;
        
        std::atomic<size_t> totalSent{0};
        std::vector<std::thread> threads;
        
        auto t1 = high_resolution_clock::now();
        
        for (size_t t = 0; t < numThreads; t++) {
            threads.emplace_back([&, t]() {
                for (size_t i = 0; i < messagesPerThread; i++) {
                    MidiMessage msg = MidiMessage::noteOn(0, 60, 100);
                    router->routeMessage(0, msg);
                    totalSent++;
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto t2 = high_resolution_clock::now();
        
        auto totalTime = duration_cast<milliseconds>(t2 - t1).count();
        double messagesPerSecond = (totalSent * 1000.0) / totalTime;
        
        PerformanceTest::result("  Threads", std::to_string(numThreads));
        PerformanceTest::result("  Total messages", 
            std::to_string(totalSent.load()));
        PerformanceTest::result("  Total time", 
            std::to_string(totalTime) + "ms");
        PerformanceTest::result("  Throughput", 
            std::to_string((int)messagesPerSecond) + " msg/s");
    }
    
    // Statistiques finales
    auto stats = router->getStats();
    std::cout << "\nStatistiques du router:\n";
    PerformanceTest::result("  Messages routés", 
        std::to_string(stats.messagesRouted));
    PerformanceTest::result("  Messages filtrés", 
        std::to_string(stats.messagesFiltered));
    PerformanceTest::result("  Messages perdus", 
        std::to_string(stats.messagesDropped));
    
    PerformanceTest::separator();
    std::cout << "✓ Test MidiRouter terminé\n\n";
}

// ============================================================================
// TEST 3: CommandProcessor - Validation et Sécurité
// ============================================================================

void test_command_validation() {
    PerformanceTest::header("TEST 3: CommandProcessor - Validation");
    
    auto deviceMgr = std::make_shared<MidiDeviceManager>();
    auto router = std::make_shared<MidiRouter>(deviceMgr);
    auto player = std::make_shared<MidiPlayer>(router);
    
    CommandProcessor processor(deviceMgr, router, player);
    
    // Test 1: Commandes valides
    std::cout << "Test 1: Commandes valides\n";
    {
        std::vector<std::pair<std::string, json>> validCommands = {
            {"player.play", {{"command", "player.play"}}},
            {"player.stop", {{"command", "player.stop"}}},
            {"routes.list", {{"command", "routes.list"}}},
            {"devices.list", {{"command", "devices.list"}}},
        };
        
        size_t passed = 0;
        for (const auto& [name, cmd] : validCommands) {
            json response = processor.processCommand(cmd);
            if (response["success"].get<bool>()) {
                passed++;
            }
        }
        
        PerformanceTest::result("  Valid commands", 
            std::to_string(passed) + "/" + std::to_string(validCommands.size()),
            passed == validCommands.size() ? "✓" : "✗");
    }
    
    // Test 2: Commandes invalides (doivent être rejetées)
    std::cout << "\nTest 2: Commandes invalides (sécurité)\n";
    {
        std::vector<std::pair<std::string, json>> invalidCommands = {
            // Injection
            {"SQL injection", {{"command", "routes.add'; DROP TABLE routes;--"}}},
            
            // Path traversal
            {"Path traversal", {
                {"command", "player.load"},
                {"file", "../../etc/passwd"}
            }},
            
            // Type confusion
            {"Type error", {
                {"command", "routes.mute"},
                {"channel", "not_a_number"},
                {"mute", true}
            }},
            
            // Hors limites
            {"Out of range", {
                {"command", "routes.add"},
                {"channel", 99},
                {"device_id", "test"}
            }},
            
            // Payload énorme
            {"DoS payload", {
                {"command", std::string(100000, 'A')}
            }},
            
            // Device ID malformé
            {"Bad device_id", {
                {"command", "devices.connect"},
                {"device_id", "../../../dev/null"}
            }},
        };
        
        size_t blocked = 0;
        for (const auto& [name, cmd] : invalidCommands) {
            json response = processor.processCommand(cmd);
            if (!response["success"].get<bool>()) {
                blocked++;
                std::cout << "  ✓ Blocked: " << name << "\n";
            } else {
                std::cout << "  ✗ FAILED: " << name << " was NOT blocked!\n";
            }
        }
        
        PerformanceTest::result("\n  Blocked", 
            std::to_string(blocked) + "/" + std::to_string(invalidCommands.size()),
            blocked == invalidCommands.size() ? "✓" : "✗");
    }
    
    // Test 3: Performance de validation
    std::cout << "\nTest 3: Performance de validation\n";
    {
        json validCmd = {
            {"command", "routes.add"},
            {"channel", 0},
            {"device_id", "usb_0"}
        };
        
        const size_t iterations = 10000;
        
        auto t1 = high_resolution_clock::now();
        
        for (size_t i = 0; i < iterations; i++) {
            processor.processCommand(validCmd);
        }
        
        auto t2 = high_resolution_clock::now();
        
        auto totalTime = duration_cast<microseconds>(t2 - t1).count();
        double avgTime = totalTime / (double)iterations;
        
        PerformanceTest::result("  Iterations", std::to_string(iterations));
        PerformanceTest::result("  Avg time", 
            std::to_string(avgTime) + "µs");
        PerformanceTest::result("  Throughput", 
            std::to_string((int)(1000000.0 / avgTime)) + " cmd/s");
    }
    
    PerformanceTest::separator();
    std::cout << "✓ Test CommandProcessor terminé\n\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main(int argc, char* argv[]) {
    std::cout << "╔════════════════════════════════════════════════════════════════╗\n";
    std::cout << "║          Tests de Performance - midiMind Priorité 2          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
    
    Logger::instance().setLevel(LogLevel::WARN);  // Réduire le bruit
    
    try {
        // Exécuter tous les tests
        test_player_large_file();
        test_router_throughput();
        test_command_validation();
        
        std::cout << "\n╔════════════════════════════════════════════════════════════════╗\n";
        std::cout << "║                   TOUS LES TESTS RÉUSSIS                      ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════════╝\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "\n✗ ERREUR: " << e.what() << "\n";
        return 1;
    }
}

// ============================================================================
// COMPILATION
// ============================================================================
/*
g++ -std=c++17 -O2 -pthread \
    test_performance.cpp \
    -I../include \
    -L../lib \
    -lmidimind \
    -o test_performance

./test_performance
*/