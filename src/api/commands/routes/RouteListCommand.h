// ============================================================================
// FICHIER 9/62: src/api/commands/routes/RouteListCommand.h
// ============================================================================

class RouteListCommand : public BaseCommand {
public:
    RouteListCommand(const json& params, std::shared_ptr<MidiRouter> router)
        : BaseCommand(params), router_(router) {}
    
    std::string getName() const override { return "routes.list"; }
    std::string getDescription() const override {
        return "List all configured MIDI routes";
    }
    
    bool validate(std::string& error) const override {
        return true; // Pas de paramètres
    }
    
    json execute() override {
        auto routes = router_->getRoutes();
        
        json routesJson = json::object();
        
        for (const auto& [channel, routeList] : routes) {
            json channelRoutes = json::array();
            
            for (const auto& route : routeList) {
                json r;
                r["device_id"] = route.deviceId;
                r["offset_ms"] = route.offsetMs;
                r["muted"] = route.muted;
                r["solo"] = route.solo;
                r["volume"] = route.volume;
                channelRoutes.push_back(r);
            }
            
            routesJson[std::to_string(channel)] = channelRoutes;
        }
        
        json response = jsonSuccess();
        response["routes"] = routesJson;
        
        return response;
    }

private:
    std::shared_ptr<MidiRouter> router_;
};