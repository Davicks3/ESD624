#include <portaudio.h>
#include <vector>
#include <fstream>
#include <sstream>
#include <iostream>
#include <atomic>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>
#include <cmath>
#include <thread>
#include <string>
#include <pthread.h>

// -------------------------
// PRIORITY CONTROL
// -------------------------
void setThreadLowPriority() {
    sched_param sch;
    sch.sched_priority = 0;
    pthread_setschedparam(pthread_self(), SCHED_OTHER, &sch);
}


// -------------------------
// Biquad filter structure
// -------------------------
struct Biquad {
    double b0, b1, b2;
    double a1, a2;
    float z1 = 0.0, z2 = 0.0;

    //process one sample (using Direct Form I transposed)
    float process(float x) {
        float xd = x;
        float y  = b0 * xd + z1;
        z1 = (float)b1 * xd - a1 * y + z2;
        z2 = (float)b2 * xd - a2 * y;
        return (float)y;
    }
};


// -------------------------
// Load biquad coefficients from file
// -------------------------
bool loadBiquads(const std::string& filename, std::vector<Biquad>& biquads, int maxBiquads){

    //Open file, with name "filename"
    std::ifstream file(filename, std::ios::binary);
    if (!file.is_open()) {
        std::cerr << "Could not open " << filename << "\n";
        return false;
    }

    // Read entire file into string
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

    // Split on '\n'
    size_t pos = 0;
    int lineNo = 0;

    // Split the string with ALL coeficients and save it.
    while (pos < content.size() && (int)biquads.size() < maxBiquads) {
        size_t nl = content.find('\n', pos);
        std::string line = (nl == std::string::npos) ? content.substr(pos)
                                                     : content.substr(pos, nl - pos);
        pos = (nl == std::string::npos) ? content.size() : nl + 1;

        // Trim trailing CR if present
        if (!line.empty() && line.back() == '\r') line.pop_back();

        // Skip empty lines
        auto is_blank = [](const std::string &s){
            for (char c : s) if (!isspace((unsigned char)c)) return false;
            return true;
        };
        if (is_blank(line)) { ++lineNo; continue; }

        // Debug print
        std::cout << "LINE[" << lineNo << "]: '" << line << "'\n";

        // Parse using stod to be strict about numeric format
        std::istringstream ss(line);
        double a0, a1, a2, b0, b1, b2;
        if (!(ss >> a0 >> a1 >> a2 >> b0 >> b1 >> b2)) {
            std::cout << "FAILED TO PARSE LINE " << lineNo << ": [" << line << "]\n";
            ++lineNo;
            continue; // keep going — don't abort on one bad line
        }

        // validation of numbers
        if (!std::isfinite(a0) || !std::isfinite(a1) || !std::isfinite(a2) ||
            !std::isfinite(b0) || !std::isfinite(b1) || !std::isfinite(b2)) {
            std::cout << "NON-FINITE VALUE ON LINE " << lineNo << "\n";
            ++lineNo;
            continue;
        }

        // Store biquads
        Biquad bq;
        bq.b0 = b0;
        bq.b1 = b1;
        bq.b2 = b2;
        bq.a1 = a1;
        bq.a2 = a2;
        bq.z1 = bq.z2 = 0.0;

        //Save biquad struct in array
        biquads.push_back(bq);

        //Go to next line
        ++lineNo;
    }

    //Debug print, print the amount of loaded biquads 
    std::cout << "Loaded " << biquads.size() << " biquads\n";
    return true;
}

// -------------------------
// DSP state container
// -------------------------
struct DSPState {
    std::vector<Biquad> biquads;
    std::atomic<bool> bypass { false };
};


// -------------------------
// PortAudio audio callback
// -------------------------
static int audioCallback(const void* inputBuffer, void* outputBuffer, unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo*, PaStreamCallbackFlags, void* userData){

    auto* state = static_cast<DSPState*>(userData);
    const float* in  = static_cast<const float*>(inputBuffer);
    float* out       = static_cast<float*>(outputBuffer);

    //Read bybass flag
    bool bypass = state->bypass.load(std::memory_order_relaxed);

    //Process each sample
    for (unsigned long i = 0; i < framesPerBuffer; ++i) {
        float x = in ? in[i] : 0.0f;

        if (bypass) {
            out[i] = x;     //Bypass flag pass through
        } else {
            float y = x;
            for (auto& bq : state->biquads)
                y = bq.process(y);     //apply filter chain
            out[i] = y;
        }
    }

    return paContinue;
}

// -------------------------
// Open serial port, read controller
// -------------------------
int openSerialPort(const char* device, int baud = B9600) {

    //Open device
    int fd = open(device, O_RDWR | O_NOCTTY);
    if (fd < 0) {
        std::cerr << "Could not open serial port " << device << "\n";
        return -1;
    }

    termios tty{};
    tcgetattr(fd, &tty);

    //Configue baud rate
    cfsetispeed(&tty, baud);
    cfsetospeed(&tty, baud);

    // 8N1 mode (8 data bits (LSB first), N = No parity bit, Stop bit (1))
    tty.c_cflag &= ~PARENB;
    tty.c_cflag &= ~CSTOPB;
    tty.c_cflag &= ~CSIZE;
    tty.c_cflag |= CS8;
    tty.c_cflag |= CREAD | CLOCAL;

    //Raw mode
    tty.c_lflag = 0;
    tty.c_iflag = 0;
    tty.c_oflag = 0;

    tty.c_cc[VMIN] = 1;
    tty.c_cc[VTIME] = 0;

    //Apply settings
    tcsetattr(fd, TCSANOW, &tty);

    return fd;
}

// -------------------------
// Serial thread
// -------------------------
void serialThread(DSPState* state) {
    setThreadLowPriority();  // <-- ensures audio ALWAYS wins

    //What is the controller name
    const char* dev = "/dev/ttyACM0";
    int fd = openSerialPort(dev,B9600);

    //If controller is not plugged in, exit
    if (fd < 0) {
        std::cerr << "Serial thread exiting (no device)\n";
        return;
    }

    std::string buffer;
    char ch;

    //Main serial loop
    while (true) {
        //Read one byte
        int n = read(fd, &ch, 1);
        if (n > 0) {
            //End of line
            if (ch == '\n') {
                std::istringstream ss(buffer);
                double a,b,c,d,e,f,g;
                int switchState;

                // Parse incomming values (all values are not used exept switchstate)
                if (ss >> a >> b >> c >> d >> e >> f >> g >> switchState) {
                    bool newBypass = (switchState == 0);
                    state->bypass.store(newBypass);
                    std::cout << "Serial: Filters " << (newBypass ? "OFF" : "ON") << std::endl;

                }

                buffer.clear();
            } else {
                //Accumulate characters
                buffer.push_back(ch);
            }
        }

        usleep(100); // 1 ms sleep, passive (reduce cpu load)
    }
}

// -------------------------
// Configure non-blocking keyboard input
// -------------------------
void setNonBlockingInput() {
    termios ttystate;
    tcgetattr(STDIN_FILENO, &ttystate);

    //Disable canonical mode + echo
    ttystate.c_lflag &= ~ICANON;
    ttystate.c_lflag &= ~ECHO;
    tcsetattr(STDIN_FILENO, TCSANOW, &ttystate);

    //Set non-blocking flag
    int flags = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, flags | O_NONBLOCK);
}

int main() {
    DSPState state;

    int maxBiquads = 111;

    //Load filter coefficients
    if (!loadBiquads("BiquadCoe.txt", state.biquads, maxBiquads)) {
        return 1;
    }

    //Initialize PortAudio
    Pa_Initialize();

    //Configure audio devices (input and output)
    PaStreamParameters inputParams{}, outputParams{};
    inputParams.device = 0;
    inputParams.channelCount = 1;
    inputParams.sampleFormat = paFloat32;
    inputParams.suggestedLatency =
        Pa_GetDeviceInfo(inputParams.device)->defaultLowInputLatency;

    outputParams.device = 0;
    outputParams.channelCount = 1;
    outputParams.sampleFormat = paFloat32;
    outputParams.suggestedLatency =
        Pa_GetDeviceInfo(outputParams.device)->defaultLowOutputLatency;

    PaStream* stream = nullptr;

    //Open audio stream
    PaError err = Pa_OpenStream(
        &stream,
        &inputParams,
        &outputParams,
        192000,   // 192 kHz
        256,
        paNoFlag,
        audioCallback,
        &state
    );

    //Check for errors
    if (err != paNoError) {
        std::cerr << "Pa_OpenStream ERROR: " << Pa_GetErrorText(err) << "\n";
        return 1;
    }

    //Start audio
    err = Pa_StartStream(stream);
    if (err != paNoError) {
        std::cerr << "Pa_StartStream ERROR: " << Pa_GetErrorText(err) << "\n";
        return 1;
    }

    //Setup keyboard (For filter state, incase no controller is plugged in)
    setNonBlockingInput();
    std::cout << "Running DSP... press 'b' to toggle filters, 'q' to quit\n";
    

    //Start serial thread
    std::thread t(serialThread, &state);
    t.detach();
    
    //Keyboard loop
    while (true) {
        char c;

        //Read key
        if (read(STDIN_FILENO, &c, 1) > 0) {

            //Toggle bypass
            if (c == 'b') {
                state.bypass = !state.bypass.load();
                std::cout << "Filters: " << (state.bypass ? "OFF" : "ON") << "\n";
            }
            //Quit(Broken, use "ctrl+C")
            if (c == 'q') break;
        }
        Pa_Sleep(20);
    }

    Pa_StopStream(stream);
    Pa_Terminate();
    return 0;
}

