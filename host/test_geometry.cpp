#include "../firmware/src/GeometryEngine.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <filesystem>
#include <algorithm>

namespace fs = std::filesystem;
using namespace DigitalSlate;

// Simple lightweight JSON parser specifically for stroke format
struct StrokeJsonDoc {
    std::string filename;
    std::string label;
    std::string description;
    std::vector<std::vector<Point2D>> strokes;
};

bool parseStrokeJson(const std::string& path, StrokeJsonDoc& doc) {
    std::ifstream file(path);
    if (!file.is_open()) return false;

    std::string content((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();

    // Extract label
    size_t labelPos = content.find("\"label\"");
    if (labelPos != std::string::npos) {
        size_t quote1 = content.find('\"', labelPos + 7);
        size_t quote2 = content.find('\"', quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos) {
            doc.label = content.substr(quote1 + 1, quote2 - quote1 - 1);
        }
    }

    // Extract description
    size_t descPos = content.find("\"description\"");
    if (descPos != std::string::npos) {
        size_t quote1 = content.find('\"', descPos + 13);
        size_t quote2 = content.find('\"', quote1 + 1);
        if (quote1 != std::string::npos && quote2 != std::string::npos) {
            doc.description = content.substr(quote1 + 1, quote2 - quote1 - 1);
        }
    }

    // Extract points per stroke
    size_t strokesPos = content.find("\"strokes\"");
    if (strokesPos == std::string::npos) return false;

    size_t curPos = strokesPos;
    while ((curPos = content.find("\"points\"", curPos)) != std::string::npos) {
        size_t arrStart = content.find('[', curPos);
        size_t arrEnd = content.find(']', arrStart);
        if (arrStart == std::string::npos || arrEnd == std::string::npos) break;

        std::string ptsBlock = content.substr(arrStart, arrEnd - arrStart + 1);
        std::vector<Point2D> currentStroke;

        size_t pPos = 0;
        while ((pPos = ptsBlock.find('{', pPos)) != std::string::npos) {
            size_t pEnd = ptsBlock.find('}', pPos);
            if (pEnd == std::string::npos) break;
            std::string ptObj = ptsBlock.substr(pPos, pEnd - pPos + 1);

            int x = 0, y = 0, t = 0;
            size_t xKey = ptObj.find("\"x\"");
            size_t yKey = ptObj.find("\"y\"");
            size_t tKey = ptObj.find("\"t\"");

            if (xKey != std::string::npos) {
                size_t colon = ptObj.find(':', xKey);
                x = std::stoi(ptObj.substr(colon + 1));
            }
            if (yKey != std::string::npos) {
                size_t colon = ptObj.find(':', yKey);
                y = std::stoi(ptObj.substr(colon + 1));
            }
            if (tKey != std::string::npos) {
                size_t colon = ptObj.find(':', tKey);
                t = std::stoi(ptObj.substr(colon + 1));
            }

            currentStroke.emplace_back(static_cast<int16_t>(x), static_cast<int16_t>(y), static_cast<uint32_t>(t));
            pPos = pEnd + 1;
        }

        doc.strokes.push_back(currentStroke);
        curPos = arrEnd + 1;
    }

    return true;
}

const char* verdictToString(TriggerVerdict v) {
    switch (v) {
        case TriggerVerdict::TRIGGER_DETECTED: return "TRIGGER";
        case TriggerVerdict::NO_TRIGGER:       return "NO_TRIGGER";
        case TriggerVerdict::AMBIGUOUS:        return "AMBIGUOUS";
        default:                               return "UNKNOWN";
    }
}

int main(int argc, char** argv) {
    std::cout << "================================================================================\n";
    std::cout << "  DIGITAL SLATE CALCULATOR - P4 TRIGGER-LINE & GEOMETRY EVALUATION  \n";
    std::cout << "================================================================================\n";

    std::string testsetDir = "data/p4_testset";
    if (argc > 1) {
        testsetDir = argv[1];
    }

    std::vector<std::string> fileList;
    for (const auto& entry : fs::directory_iterator(testsetDir)) {
        if (entry.path().extension() == ".json") {
            fileList.push_back(entry.path().string());
        }
    }
    std::sort(fileList.begin(), fileList.end());

    if (fileList.empty()) {
        std::cerr << "[ERROR] No test files found in: " << testsetDir << "\n";
        return 1;
    }

    std::cout << "Loaded " << fileList.size() << " test files from: " << testsetDir << "\n\n";

    int totalTriggers = 0;
    int totalNonTriggers = 0;
    int totalAmbiguous = 0;

    int triggerHits = 0;
    int triggerMisses = 0;
    int falseTriggers = 0;
    int ambiguousDetected = 0;
    int correctDecisions = 0;

    std::cout << std::left
              << std::setw(4)  << "#"
              << std::setw(34) << "Filename"
              << std::setw(13) << "GroundTruth"
              << std::setw(15) << "Prediction"
              << std::setw(8)  << "Match"
              << "Diagnostic Reason\n";
    std::cout << "--------------------------------------------------------------------------------\n";

    for (size_t i = 0; i < fileList.size(); ++i) {
        StrokeJsonDoc doc;
        fs::path p(fileList[i]);
        doc.filename = p.filename().string();

        if (!parseStrokeJson(fileList[i], doc)) {
            std::cerr << "Failed to parse: " << doc.filename << "\n";
            continue;
        }

        TriggerResult res = GeometryEngine::detectTriggerLine(doc.strokes);
        std::string predStr = verdictToString(res.verdict);

        bool isCorrect = false;
        if (doc.label == "TRIGGER") {
            totalTriggers++;
            if (res.verdict == TriggerVerdict::TRIGGER_DETECTED) {
                triggerHits++;
                isCorrect = true;
            } else {
                triggerMisses++;
            }
        } else if (doc.label == "AMBIGUOUS") {
            totalAmbiguous++;
            if (res.verdict == TriggerVerdict::AMBIGUOUS) {
                ambiguousDetected++;
                isCorrect = true;
            }
        } else {
            // MINUS or ACCIDENTAL
            totalNonTriggers++;
            if (res.verdict == TriggerVerdict::TRIGGER_DETECTED) {
                falseTriggers++;
            } else {
                isCorrect = true;
            }
        }

        if (isCorrect) correctDecisions++;

        std::cout << std::left
                  << std::setw(4)  << (i + 1)
                  << std::setw(34) << doc.filename
                  << std::setw(13) << doc.label
                  << std::setw(15) << predStr
                  << std::setw(8)  << (isCorrect ? "[OK]" : "[FAIL]")
                  << res.reason << "\n";
    }

    std::cout << "--------------------------------------------------------------------------------\n";

    float falseTriggerRate = (totalNonTriggers > 0) ? (float)falseTriggers / totalNonTriggers * 100.0f : 0.0f;
    float missRate = (totalTriggers > 0) ? (float)triggerMisses / totalTriggers * 100.0f : 0.0f;
    float ambiguousRecall = (totalAmbiguous > 0) ? (float)ambiguousDetected / totalAmbiguous * 100.0f : 0.0f;
    float overallAccuracy = (float)correctDecisions / fileList.size() * 100.0f;

    std::cout << "\n============================= EVALUATION METRICS =============================\n";
    std::cout << "Total Test Files Analyzed   : " << fileList.size() << "\n";
    std::cout << "Ground-Truth Class Breakdown: "
              << totalTriggers << " Triggers, "
              << totalNonTriggers << " Non-Triggers (Minus/Accidental), "
              << totalAmbiguous << " Ambiguous\n";
    std::cout << "------------------------------------------------------------------------------\n";
    std::cout << "Trigger Detections (Hits)   : " << triggerHits << " / " << totalTriggers << "\n";
    std::cout << "Trigger Misses              : " << triggerMisses << " / " << totalTriggers << "\n";
    std::cout << "False Triggers (False Pos)  : " << falseTriggers << " / " << totalNonTriggers << "\n";
    std::cout << "Ambiguous Correctly Flagged : " << ambiguousDetected << " / " << totalAmbiguous << "\n";
    std::cout << "------------------------------------------------------------------------------\n";
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Measured Miss Rate          : " << missRate << "%\n";
    std::cout << "Measured False-Trigger Rate : " << falseTriggerRate << "%\n";
    std::cout << "Ambiguity Recognition Rate  : " << ambiguousRecall << "%\n";
    std::cout << "Overall Classification Acc. : " << overallAccuracy << "%\n";
    std::cout << "==============================================================================\n";

    // Criteria: false trigger rate must be 0%, miss rate 0%, ambiguous cases never guessed as trigger
    bool acceptPass = (falseTriggerRate == 0.0f && missRate == 0.0f && ambiguousDetected == totalAmbiguous);
    return acceptPass ? 0 : 1;
}
