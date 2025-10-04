// ============================================================================
// src/api/observers/IApiServerObserver.h
// Observateurs spécifiques pour ApiServer
// ============================================================================

struct ApiServerEvent {
    enum class Type {
        CLIENT_CONNECTED,
        CLIENT_DISCONNECTED,
        MESSAGE_RECEIVED,
        ERROR_OCCURRED
    };
    
    Type type;
    std::string clientId;
    json data;
    std::string message;
};

class IApiServerObserver : public IObserver<ApiServerEvent> {
public:
    virtual ~IApiServerObserver() = default;
    
    // Méthodes optionnelles spécifiques
    virtual void onClientConnected(const std::string& clientId) {}
    virtual void onClientDisconnected(const std::string& clientId) {}
    virtual void onMessageReceived(const std::string& clientId, const json& message) {}
    virtual void onError(const std::string& error) {}
    
    // Implémentation de IObserver
    void onNotify(const ApiServerEvent& event) override {
        switch (event.type) {
            case ApiServerEvent::Type::CLIENT_CONNECTED:
                onClientConnected(event.clientId);
                break;
            case ApiServerEvent::Type::CLIENT_DISCONNECTED:
                onClientDisconnected(event.clientId);
                break;
            case ApiServerEvent::Type::MESSAGE_RECEIVED:
                onMessageReceived(event.clientId, event.data);
                break;
            case ApiServerEvent::Type::ERROR_OCCURRED:
                onError(event.message);
                break;
        }
    }
};