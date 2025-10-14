// ============================================================================
// src/utils/ResourceMonitor.h - Monitoring des ressources système
// ============================================================================
#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include "../core/Logger.h"

namespace midiMind {

struct SystemResources {
    float cpuPercent = 0.0f;
    float memoryPercent = 0.0f;
    long memoryUsedKB = 0;
    long memoryTotalKB = 0;
    float temperatureCelsius = 0.0f;
    long diskUsedKB = 0;
    long diskTotalKB = 0;
};

class ResourceMonitor {
public:
    static SystemResources getSystemResources() {
        SystemResources resources;
        
        // CPU
        resources.cpuPercent = getCpuUsage();
        
        // Mémoire
        getMemoryInfo(resources.memoryUsedKB, resources.memoryTotalKB);
        if (resources.memoryTotalKB > 0) {
            resources.memoryPercent = (float)resources.memoryUsedKB / resources.memoryTotalKB * 100.0f;
        }
        
        // Température (Raspberry Pi)
        resources.temperatureCelsius = getTemperature();
        
        // Disque
        getDiskInfo(resources.diskUsedKB, resources.diskTotalKB);
        
        return resources;
    }

private:
    static float getCpuUsage() {
        // Lecture simplifiée de /proc/stat
        std::ifstream file("/proc/stat");
        if (!file.is_open()) return 0.0f;
        
        std::string line;
        std::getline(file, line);
        
        // Format: cpu  user nice system idle iowait irq softirq
        std::istringstream ss(line);
        std::string cpu;
        long user, nice, system, idle;
        ss >> cpu >> user >> nice >> system >> idle;
        
        static long prevIdle = 0;
        static long prevTotal = 0;
        
        long total = user + nice + system + idle;
        long totalDiff = total - prevTotal;
        long idleDiff = idle - prevIdle;
        
        float usage = 0.0f;
        if (totalDiff > 0) {
            usage = 100.0f * (1.0f - (float)idleDiff / totalDiff);
        }
        
        prevIdle = idle;
        prevTotal = total;
        
        return usage;
    }
    
    static void getMemoryInfo(long& usedKB, long& totalKB) {
        std::ifstream file("/proc/meminfo");
        if (!file.is_open()) {
            usedKB = 0;
            totalKB = 0;
            return;
        }
        
        std::string line;
        long memTotal = 0, memAvailable = 0;
        
        while (std::getline(file, line)) {
            if (line.find("MemTotal:") == 0) {
                std::istringstream ss(line);
                std::string label;
                ss >> label >> memTotal;
            } else if (line.find("MemAvailable:") == 0) {
                std::istringstream ss(line);
                std::string label;
                ss >> label >> memAvailable;
            }
        }
        
        totalKB = memTotal;
        usedKB = memTotal - memAvailable;
    }
    
    static float getTemperature() {
        // Raspberry Pi: /sys/class/thermal/thermal_zone0/temp
        std::ifstream file("/sys/class/thermal/thermal_zone0/temp");
        if (!file.is_open()) return 0.0f;
        
        int temp;
        file >> temp;
        
        // La valeur est en millidegrés
        return temp / 1000.0f;
    }
    
    static void getDiskInfo(long& usedKB, long& totalKB) {
        // Utilise la commande df (simplifié)
        // En production, utiliser statvfs()
        usedKB = 0;
        totalKB = 0;
        
        FILE* pipe = popen("df / | tail -1", "r");
        if (!pipe) return;
        
        char buffer[256];
        if (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            // Format: /dev/root  total  used  available  %  /
            long total, used;
            sscanf(buffer, "%*s %ld %ld", &total, &used);
            totalKB = total;
            usedKB = used;
        }
        
        pclose(pipe);
    }
};

} // namespace midiMind