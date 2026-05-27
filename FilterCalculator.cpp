#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <numbers>
#include <chrono>
#include <complex>
#include <iomanip>
using namespace std;

// Constant for PI used in DSP calculations
constexpr float PI = 3.14159265358979323846f;

//Struct representing a single sample
struct MeasData {
    float frequency;
    float spl;

    bool operator<(const MeasData& other) const {
        return spl < other.spl;
    }
};


struct biquadCoeff {
    double b0, b1, b2;
    double a0, a1, a2;
};

enum class biquadType { peak, lowShelf, highShelf, notch };

struct biquad {
    biquadType type;
    bool valid = false;
    float f0;
    float Q;
    float gainDB;
    biquadCoeff coeff;
};

//-----------------------------------------------------------------//
//                   MeasData processing FUNCTIONS                 //
//-----------------------------------------------------------------//
std::vector<MeasData> readData(std::string file)                            //readData funktion med "file" som input
{                                                                           
    std::ifstream readFile(file);                                           //Bruger biliotek "fstream" til at indlæse den given fil
    std::vector<MeasData> allData;                                          //Opretter et struct vector(array) til obevaring af data fra fil

    if (!readFile.is_open()) {                                              //Error tjek for om filen eksisterer
        std::cerr << "ERROR, no file or couldent open it" << std::endl;     //Print Errror hvis fil ikke eksiterer
        return allData;                                                     //Retuner ud af funktion med tomt array
    }

    std::string line;                                                       
    while (std::getline(readFile, line)) {                                  //Udpluk en linje fra filen og gem i "line"
        std::stringstream ss(line);                                         //converter string til en stream
        MeasData m;                                                         //Opret midlertidig struct "Measdata" variabel "m"
        if (float tmp; ss >> m.frequency >> m.spl >> tmp) {                 //Opret "tmp" variabel til fase, indsæt i "m"
            allData.push_back(m);                                           //Skub "m" ind på bagerste plads i array
        }
    }
    readFile.close();                                                       //Er hele filen læst luk filen

    return allData;                                                         //Retuner vector med frekvensrespons
}


std::vector<MeasData> measAvgFunc(const std::vector<std::string>& allFiles) {
    std::vector<std::vector<MeasData>> allFilesData;

    // Load all files
    for (const auto& f : allFiles) {
        allFilesData.push_back(readData(f));
    }

    // Use the first file as reference
    std::vector<MeasData> measAvg = allFilesData[0];
    int fileCount = allFilesData.size();
    int pointCount = measAvg.size();

    // Average SPL across all files
    for (int i = 0; i < pointCount; i++) {    // Loop der går over alle målepunkter (samme indeks i alle filer)
        double sum = 0.0;                     // Initialiser en sum-variabel til at akkumulere SPL-værdier for dette punkt

        for (int f = 0; f < fileCount; f++) { // Loop over alle filer (måleserier)
            sum += allFilesData[f][i].spl;    // Læg SPL-værdien for punkt i fra fil f til summen
        }

        measAvg[i].spl = sum / fileCount;     // Beregn gennemsnittet for punkt i og gem det i measAvg
    }

    return measAvg;
}

//SavetoFile, writes a vector of MeasData struct to a text file.
void saveToFile(const std::vector<MeasData>& data, const std::string& filename) {
    //Path to place the file, with the custom name "filename"
    std::string fullpath = "FilterCalcOutput/" + filename;
    
    // Open/Create the file at the output location 
    std::ofstream out(fullpath);
    if (!out.is_open()) {
        //If file cannot open give error
        std::cerr << "ERROR OPENING '" << fullpath << "'" << std::endl;
        return;
    }

    //Write each meas as a line formatted: Frequency SPL
    for (const auto& m : data) {
        out << m.frequency << " " << m.spl << "\n";
    }

    //Close the file
    out.close();
}

//SmoothData, applies a octave smoothing (1/N) to a measured frequency response (data)
std::vector<MeasData> smoothData(const std::vector<MeasData>& data, float N) {
    //copy data to preserve frequencies
    std::vector<MeasData> out = data;
    if (data.empty()) return out;

    //loop over each sample
    for (size_t i = 0; i < data.size(); i++) {
        float f0 = data[i].frequency;

        //Fractional octave smoothing factor, for 1/N octave smoothing, window width = +-1/(2*N).
        float factor = std::pow(2.0f, 1.0f / (2.0f * N));
        
        //calculate the window around center frequency
        float fLow  = f0 / factor;
        float fHigh = f0 * factor;

        float sum = 0.0f;
        int count = 0;

        //Sam SPL values inside the smoothing window
        for (size_t j = 0; j < data.size(); j++) {
            float f = data[j].frequency;

            //Check if sample lies inside the window
            if (f >= fLow && f <= fHigh) {
                sum += data[j].spl;
                count++;
            }
        }
        // Replace the SPL value with the local average inside the window
        if (count > 0)
            out[i].spl = sum / count;
    }

    return out;
}

//Baseline, calculate the median SPL value of a frequency response (data)
float baseline(const std::vector<MeasData>& data) {
    //copy input, to sort without modifying the original data
    std::vector<MeasData> splSort = data;
    //sort by SPL lowest first
    std::sort(splSort.begin(), splSort.end(),
              [](const MeasData& a, const MeasData& b) { return a.spl < b.spl; });
    //return the SPL value
    return splSort[splSort.size() / 2].spl;
}

//findDeviation, find calculate the deviation curve by subtractiong the baseline SPL from each point
std::vector<MeasData> findDeviation(const std::vector<MeasData>& data) {
    //Copy the input to preserve the original data.
    std::vector<MeasData> dev = data;
    //compute baseline
    float bl = baseline(data);
    //subtract the baseline from each SPL value
    std::cout << "baseline: " << bl << std::endl;
    for (size_t i = 0; i < data.size(); i++) {
        dev[i].spl = data[i].spl - bl;
    }
    return dev;
}

//-----------------------------------------------------------------//
//                          FILTER FUNCTIONS                       //
//-----------------------------------------------------------------//
// THANKS TO RBJ for the EQ-CookBook
// https://www.w3.org/TR/audio-eq-cookbook/

//makePeak, calculates the biquad coefficients for a peaking EQ filter.
biquadCoeff makePeak(float f0, float Q, float gainDB, float fs)
{
    biquadCoeff c{};

    //Convert gain in dB to linear amplitude factor.
    double A  = std::pow(10.0, gainDB / 40.0);
    //Normalized angular frequency
    double w0 = 2.0 * (double)PI * (double)f0 / (double)fs;
    //Precompute cos and sin of the angular frequency
    double cw = std::cos(w0);
    double sw = std::sin(w0);

    // bandwitdh control term
    double alpha = sw / (2.0 * (double)Q);

    //RBJ peak filter formulas
    c.b0 = 1.0 + alpha * A;
    c.b1 = -2.0 * cw;
    c.b2 = 1.0 - alpha * A;
    c.a0 = 1.0 + alpha / A;
    c.a1 = -2.0 * cw;
    c.a2 = 1.0 - alpha / A;

    //Normalize so a0 = 1
    c.b0 /= c.a0;
    c.b1 /= c.a0;
    c.b2 /= c.a0;
    c.a1 /= c.a0;
    c.a2 /= c.a0;
    c.a0 = 1.0;

    return c;
}

//makeLowShef, same as for peaking but using the RBJ formulas for lowShelf
biquadCoeff makeLowShef(float f0, float gainDB, float slope, float fs) {
    biquadCoeff c{};

    double A  = std::pow(10.0, gainDB / 40.0);
    double w0 = 2.0 * (double)PI * (double)f0 / (double)fs;
    double cw = std::cos(w0);
    double sw = std::sin(w0);

    double alpha = sw / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / (double)slope - 1.0) + 2.0);
    double temp  = 2.0 * std::sqrt(A) * alpha;

    c.b0 =    A * ((A + 1.0) - (A - 1.0) * cw + temp);
    c.b1 =  2.0 * A * ((A - 1.0) - (A + 1.0) * cw);
    c.b2 =    A * ((A + 1.0) - (A - 1.0) * cw - temp);
    c.a0 =        (A + 1.0) + (A - 1.0) * cw + temp;
    c.a1 =   -2.0 * ((A - 1.0) + (A + 1.0) * cw);
    c.a2 =        (A + 1.0) + (A - 1.0) * cw - temp;

    c.b0 /= c.a0;
    c.b1 /= c.a0;
    c.b2 /= c.a0;
    c.a1 /= c.a0;
    c.a2 /= c.a0;
    c.a0 = 1.0;

    return c;
}

//makeHighShef, same as for peaking but using the RBJ formulas for highShelf
biquadCoeff makeHighShef(float f0, float gainDB, float slope, float fs) {
    biquadCoeff c{};

    double A  = std::pow(10.0, gainDB / 40.0);
    double w0 = 2.0 * (double)PI * (double)f0 / (double)fs;
    double cw = std::cos(w0);
    double sw = std::sin(w0);

    double alpha = sw / 2.0 * std::sqrt((A + 1.0 / A) * (1.0 / (double)slope - 1.0) + 2.0);
    double temp  = 2.0 * std::sqrt(A) * alpha;

    c.b0 =    A * ((A + 1.0) + (A - 1.0) * cw + temp);
    c.b1 = -2.0 * A * ((A - 1.0) + (A + 1.0) * cw);
    c.b2 =    A * ((A + 1.0) + (A - 1.0) * cw - temp);
    c.a0 =        (A + 1.0) - (A - 1.0) * cw + temp;
    c.a1 =    2.0 * ((A - 1.0) - (A + 1.0) * cw);
    c.a2 =        (A + 1.0) - (A - 1.0) * cw - temp;

    c.b0 /= c.a0;
    c.b1 /= c.a0;
    c.b2 /= c.a0;
    c.a1 /= c.a0;
    c.a2 /= c.a0;
    c.a0 = 1.0;

    return c;
}

//makeNotch, same as for peaking but using the RBJ formulas for Notch
biquadCoeff makeNotch(float f0, float Q, float fs) {
    biquadCoeff c{};

    double w0 = 2.0 * (double)PI * (double)f0 / (double)fs;
    double cw = std::cos(w0);
    double sw = std::sin(w0);
    double alpha = sw / (2.0 * (double)Q);

    c.b0 =  1.0;
    c.b1 = -2.0 * cw;
    c.b2 =  1.0;
    c.a0 =  1.0 + alpha;
    c.a1 = -2.0 * cw;
    c.a2 =  1.0 - alpha;

    c.b0 /= c.a0;
    c.b1 /= c.a0;
    c.b2 /= c.a0;
    c.a1 /= c.a0;
    c.a2 /= c.a0;
    c.a0 = 1.0;

    return c;
}

//-----------------------------------------------------------------//
//                 Detection / Sequential Filtering                //
//-----------------------------------------------------------------//

//detectBassRollOff, detect the roll-off frequency in the deviation curve
float detectBassRollOff(const std::vector<MeasData>& dev)
{
    // Minimum dB drop across the window
    const float minDrop = -6.0f;
    // Minimum negative slope in the window (dB/hz)
    const float minSlope = -0.02f;
    //window size in samples
    const int window = 20;

    //scan the dev curve from window to the size of the dev curve.
    for (size_t i = window; i < dev.size(); i++)
    {
        //Difference in SPL across the window
        float dy = dev[i].spl - dev[i - window].spl;
        //Difference en frequency across the window
        float dx = dev[i].frequency - dev[i - window].frequency;

        //calculate the slope
        float slope = dy / dx;

        //Check if the conditions indicate a roll-off
        if (dy < minDrop && slope < minSlope)
            return dev[i - window].frequency;
    }
    
    return 0.0f;
}

//detectSingleFilter Detect the largest deviation peak, and calculates the biquad coefficients.
biquad detectSingleFilter(const std::vector<MeasData>& curve, float fs, float rollOffFreq, float tollerace)
{
    // variable to track the largest deviation
    float maxAbs = 0.0f;
    //Index of the detected peak
    size_t idx = 0;

    //Find the largest absolute deviation above roll-off frequency
    for (size_t i = 1; i < curve.size() - 1; i++)
    {
        //Skip frequencies below bass roll-off
        if (curve[i].frequency < rollOffFreq)
            continue;

        float a = std::abs(curve[i].spl);
        
        //track absolute deviation
        if (a > maxAbs)
        {
            maxAbs = a;
            idx = i;
        }
    }

    // if deviation is too small no filter is needed 
    if (maxAbs < tollerace) {
        biquad b;
        b.valid = false;
        return b;
    }

    //If filter needed extract peak parameters
    //Define center frequency
    float f0 = curve[idx].frequency;
    //Calculate the gain, since the wanted is 0dB(Normalized freq response) the gain is the negative of the SPL value at the index
    float gainDB = -curve[idx].spl;
    //Clamp the gain to prevent distortion
    gainDB = std::clamp(gainDB, -8.0f, 8.0f);

    //Estimate Q factor from the half amplitude around itself
    size_t L = idx, R = idx;
    //half amplitude
    float half = curve[idx].spl * 0.5f;

    //Move left until deviation curve falls below half amplitude
    while (L > 0 && std::abs(curve[L].spl) > std::abs(half)) L--;
    //Move right until deviation curve falls below half amplitude
    while (R < curve.size() - 1 && std::abs(curve[R].spl) > std::abs(half)) R++;

    //calculate bandwidth from the left and right frequency found.
    float fLow = curve[L].frequency;
    float fHigh = curve[R].frequency;
    float BW = fHigh - fLow; 

    std::cout << curve[L].frequency << ", " << curve[L].spl << " " << curve[R].frequency << ", " << curve[R].spl << BW << std::endl;

    //Calculate the Q factor
    float Q = f0 / BW;

    //clamp Q to avoid distortion
    Q = std::clamp(Q, 0.3f, 10.0f);

    //build the filter, insert parameters found
    biquad b;
    b.valid = true;
    b.f0 = f0;
    b.Q = Q;
    b.gainDB = gainDB;
    b.type = biquadType::peak;

    //Generate BIQUAD coefficients from parameters
    b.coeff = makePeak(f0, Q, gainDB, fs);

    return b;
}

//applyFilterToCurve, applies a songle biquad filter to a frequency response
std::vector<MeasData> applyFilterToCurve(const std::vector<MeasData>& curve, const biquad& f, float fs)
{
    //Make vector for output and allocate space
    std::vector<MeasData> out;
    out.reserve(curve.size());

    for (const auto& d : curve)
    {
        //compute normalized angular frequnecy
        double w = 2.0 * (double)PI * (double)d.frequency / (double)fs;

        //Precompute cos and sin of the angular frequency
        double cw = std::cos(w);
        double sw = std::sin(w);

        //Compute complex calculation for z1 and z2
        std::complex<double> z1(cw, -sw);
        std::complex<double> z2 = z1 * z1;

        const auto& c = f.coeff;
        //calculate the transferfunctions nummerator
        std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2;
        //calculate the transferfunctions nummerator
        std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2;

        //calculate the magnutude response
        double mag = std::abs(num / den);

        //convert magnitude to dB
        float eqDB = (float)(20.0 * std::log10(mag));

        //apply EQ gain to the deviation curve
        MeasData m;
        m.frequency = d.frequency;
        //add filter effect in dB
        m.spl = d.spl + eqDB; 

        out.push_back(m);
    }

    return out;
}

//detectFiltersSequential, Iterativly detect and generate correction filters until maxfilters or within tollerance
std::vector<biquad> detectFiltersSequential(const std::vector<MeasData>& deviation, float fs, int maxFilters, float tollerance)
{
    std::vector<biquad> filters;
    std::vector<MeasData> current = deviation;

    //Detect bass roll-off once, and parse into function needing it
    float rollOff = detectBassRollOff(deviation);

    //Main itterative loop
    for (int i = 0; i < maxFilters; i++)
    {
        //Detect the larget peak/dip
        biquad f = detectSingleFilter(current, fs, rollOff, tollerance);

        //Check if the found peak is valid
        if (!f.valid)
            break;

        //Store the detected filter
        filters.push_back(f);
        std::cout << f.f0 << " " << f.gainDB << std::endl;

        //Apply the filter to the current curve
        current = applyFilterToCurve(current, f, fs);

        //Save intermidatee curve for debugging/visualization
        saveToFile(current, "iter_" + std::to_string(i) + ".txt");
    }

    return filters;
}

//-----------------------------------------------------------------//
//                        FILTER Implementation                    //
//-----------------------------------------------------------------//
//Standalone function for calculating the biquad gain at a specific frequency
float biquadGainAtFreq(const biquadCoeff& c, float freq, float fs)
{
    double w = 2.0 * (double)PI * (double)freq / (double)fs;
    
    double cw = std::cos(w);
    double sw = std::sin(w);

    std::complex<double> z1(cw, -sw);

    std::complex<double> z2 = z1 * z1;
    std::complex<double> num = c.b0 + c.b1 * z1 + c.b2 * z2;
    std::complex<double> den = 1.0 + c.a1 * z1 + c.a2 * z2;

    double mag = std::abs(num / den);

    return (float)(20.0 * std::log10(mag));
}
//Standalone function for applying filters sequectially
float applyFilterChainAtFreq(const std::vector<biquad>& filters, float freq, float fs)
{
    float total = 0.0f;

    for (const auto& f : filters)
        total += biquadGainAtFreq(f.coeff, freq, fs);

    return total;
}
//Standalone function for calculating the final corrected frequency response
std::vector<MeasData> filterImplementation(const std::vector<biquad>& filters, const std::vector<MeasData>& deviation, float fs) {
    std::vector<MeasData> corrected;
    corrected.reserve(deviation.size());

    for (const auto& d : deviation)
    {
        float eqDB = applyFilterChainAtFreq(filters, d.frequency, fs);

        MeasData c;
        c.frequency = d.frequency;
        c.spl = d.spl + eqDB;

        corrected.push_back(c);
    }

    return corrected;
}


//function so save the filterresponse.
void saveFilterResponse(const std::vector<biquad>& filters, const std::vector<MeasData>& deviation, float fs, const std::string& filename)
{
    std::string fullpath = "FilterCalcOutput/" + filename;

    std::ofstream out(fullpath);
    if (!out.is_open()) {
        std::cerr << "ERROR OPENING '" << fullpath << "'" << std::endl;
        return;
    }

    for (const auto& d : deviation) {
        float eqDB = applyFilterChainAtFreq(filters, d.frequency, fs);
        out << d.frequency << " " << eqDB << "\n";
    }

    out.close();
}



int main() {
    auto start = std::chrono::high_resolution_clock::now();

    //Files to load
    std::vector<std::string> allFiles = {
        "InputFiles/FreqResp1.txt",
        "InputFiles/FreqResp2.txt",
        "InputFiles/FreqResp3.txt",
        "InputFiles/FreqResp4.txt",
        "InputFiles/FreqResp5.txt"
    };

    //average all meas files into on frequency response
    std::vector<MeasData> measAvg = measAvgFunc(allFiles);
    saveToFile(measAvg, "measAvg.txt");

    //Smooth the averaged response
    std::vector<MeasData> smoothed = smoothData(measAvg, 6);
    saveToFile(smoothed, "smoothed.txt");

    //Compute devitation from median baseline
    std::vector<MeasData> deviation = findDeviation(smoothed);
    saveToFile(deviation, "deviation.txt");

    //Optional manual filter for evt. bass boost
    biquad shelf;
    shelf.valid = true;
    shelf.type = biquadType::lowShelf;   
    shelf.f0 = 100.0f;                   
    shelf.gainDB = 0.0f;                 
    shelf.Q = 1.2f;                      
    shelf.coeff = makePeak(shelf.f0, shelf.Q, shelf.gainDB, 192000.0f);
    std::vector<biquad> biquads;
    biquads.push_back(shelf);

    // Detect correction filters sequentially
    auto detected = detectFiltersSequential(deviation, 192000.0f, 100, 1.5f);
    biquads.insert(biquads.end(), detected.begin(), detected.end());

    //apply filters to generate corrected response
    std::vector<MeasData> corrected = filterImplementation(biquads, deviation, 192000.0f);

    //save corrected response
    saveToFile(corrected, "newResponse.txt");
    saveFilterResponse(biquads, deviation, 192000.0f, "filterResponse.txt");

    //save biquads for implementation
    std::ofstream out2("FilterCalcOutput/BiquadCoe.txt");
    if (!out2.is_open()) {
        std::cerr << "ERROR OPENING 'BiquadCoe.txt'" << std::endl;
    }
    out2 << std::setprecision(17);
    for (const auto& b : biquads) {
       out2 << b.coeff.a0 << " " << b.coeff.a1 << " " << b.coeff.a2 << " "
            << b.coeff.b0 << " " << b.coeff.b1 << " " << b.coeff.b2 << "\n";
    }
    out2.close();

    //Time the program(for debugging) and exit
    std::cout << "DONE" << std::endl;
    auto end = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> elapsed = end - start;
    std::cout << "Program took " << elapsed.count() << " seconds\n";
}


