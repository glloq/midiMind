# midiMind-Backend
code c++ pour la gestion du routage midi wif, usb et bluetooth


<pre>
  midiMind/
├── external/                       
│   ├── rtmidi/
│   ├── midifile/
│   ├── json/
│   ├── websocketpp/
│   └── asio/                      (optionnel ?)
│
├── storage/                        
│   ├── midi_files/                 # Fichiers MIDI utilisateur
│   │   ├── uploaded/               # Fichiers uploadés via API
│   │   ├── analyzed/               # Cache analyses MIDI
│   │   └── temp/                   # Uploads temporaires
│   ├── presets/                    # Presets de routage
│   └── logs/                       # Logs de debug MIDI 
│
├── src/                            
│   ├── core/                       
│   │   ├── Application.h
│   │   ├── Application.cpp
│   │   ├── Config.h               
│   │   ├── Config.cpp             
│   │   ├── Logger.h               
│   │   ├── Logger.cpp
│   │   └── ErrorManager.h
│   │
│   ├── midi/                       
│   │   ├── MidiMessage.h
│   │   ├── MidiMessage.cpp
│   │   ├── MidiRouter.h           
│   │   ├── MidiRouter.cpp         
│   │   ├── MidiPlayer.h
│   │   ├── MidiPlayer.cpp
│   │   ├── MidiFileAnalyzer.h
│   │   └── devices/               
│   │       ├── MidiDevice.h
│   │       ├── MidiDeviceManager.h
│   │       ├── MidiDeviceManager.cpp
│   │       ├── UsbMidiDevice.h
│   │       ├── UsbMidiDevice.cpp  
│   │       ├── WifiMidiDevice.h
│   │       ├── WifiMidiDevice.cpp
│   │       ├── BtMidiDevice.h
│   │       └── BtMidiDevice.cpp   
│   │
│   ├── api/                        
│   │   ├── ApiServer.h            
│   │   ├── ApiServer.cpp          
│   │   ├── CommandProcessor.h     
│   │   └── CommandProcessor.cpp   
│   │
│   ├── network/                    
│   │   ├── NetworkManager.h
│   │   └── NetworkManager.cpp
│   │
│   ├── utils/                     
│   │   ├── Preset.h
│   │   ├── Preset.cpp
│   │   ├── ResourceMonitor.h
│   │   └── ResourceMonitor.cpp
│   │
│   └── main.cpp                    
│
├── config/                         
│   └── config.json                 
│
├── scripts/                        
│   ├── build.sh                  
│   ├── install.sh                  
│   ├── setup_external.sh          
│   ├── setup_network.sh            
│   └── migrate_to_v2.sh          
│
├── docs/                           [DOCUMENTATION]
│   ├── architecture.md
│   ├── api.md
│   └── development.md
│
├── tests/                          ⭐ À CRÉER
│   ├── unit/
│   ├── integration/
│   └── CMakeLists.txt
│
├── CMakeLists.txt                  
├── README.md                       
└── .gitignore                     
</pre>
