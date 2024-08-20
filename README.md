# Features
- NFC Setup and Reading
    - scan_card(): Reads a NFC card UID and sends it to a webserver.
- Wifi Connection
    - Connects to Wifi using a ESP32 WROOM.
- API Integration
    - Sends the MAC and IP addresses of the ESP32 WROOM to a server using HTTP POST.
- State Management
    - Uses a state machine to handle different states
        - lock() is the default state and checks for a card, if it receives a authorized signal from the server, unlocks the machine.
        - unlock() unlocks the machine and switches state using a manual button.
        - overide() keeps the machine unlocked if the key connected to the overide pin (OVERIDE_KEY_PIN) is moved to the closed state.
        The access control is overiden until the key is moved to the open state.
        - disable() keeps the machine locked if the key connected to the disable pin (DISABLE_KEY_PIN) is moved to the closed state. 
        The access control is overiden until the key is moved to the open state. Disable is also turned off when the key connected the the overide pin is moved to the closed state
- Manual Controls
    -unlock(), overide(), and disable() all use a form of manual control to switch states. unlock() uses a button with built in delay to account for debounce. overide() and disable() use a key to change states.
