// ============================================================================
// FICHIER 26/62: src/api/commands/file/FileAnalyzeCommand.h
// ============================================================================

class FileAnalyzeCommand : public BaseCommand {
public:
    FileAnalyzeCommand(const json& params) : BaseCommand(params) {}
    
    std::string getName() const override { return "file.analyze"; }
    std::string getDescription() const override {
        return "Analyze a MIDI file and return detailed information";
    }
    
    json getParameterSpec() const override {
        return json::array({{
            {"name", "file"}, {"type", "string"}, {"required", true},
            {"description", "Path to MIDI file to analyze"}
        }});
    }
    
    bool validate(std::string& error) const override {
        return validateFilePath("file", error);
    }
    
    json execute() override {
        std::string filepath = params_["file"];
        
        try {
            auto analysis = MidiFileAnalyzer::analyze(filepath);
            
            json response = jsonSuccess();
            response["analysis"] = analysis.toJson();
            
            return response;
            
        } catch (const std::exception& e) {
            return jsonError("Analysis failed: " + std::string(e.what()));
        }
    }
};