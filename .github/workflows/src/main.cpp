#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <chrono>
#include <mutex>
#include <vector>

using namespace geode::prelude;

// Thread-safe storage for catching high-resolution human inputs
struct SubFrameInput {
    bool isPressed;
    int buttonPlayer; // 1 for Player 1, 2 for Player 2
    double offsetTime; // Decimal slice of the frame when clicked
};

std::chrono::high_resolution_clock::time_point lastFrameTime;
std::vector<SubFrameInput> inputQueue;
std::mutex queueMutex;

// GEAR 1: Hook the frame loop to track our 0.0 frame timeline baseline
class $modify(GJBaseGameLayer) {
    void update(float dt) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            lastFrameTime = std::chrono::high_resolution_clock::now();
        }
        GJBaseGameLayer::update(dt);
    }

    // GEAR 2: Catch human raw clicks and calculate exactly when they happened mid-frame
    void handleButton(bool press, int button, bool isPlayer1) {
        auto clickTime = std::chrono::high_resolution_clock::now();
        
        std::lock_guard<std::mutex> lock(queueMutex);
        std::chrono::duration<double> elapsed = clickTime - lastFrameTime;
        
        SubFrameInput input;
        input.isPressed = press;
        input.buttonPlayer = isPlayer1 ? 1 : 2;
        input.offsetTime = elapsed.count();

        inputQueue.push_back(input);
        
        // Let the base game loop log the input path naturally
        GJBaseGameLayer::handleButton(press, button, isPlayer1);
    }
};

// GEAR 3: Intercept physics and process the sub-steps transparently
class $modify(PlayerObject) {
    void updatePhysics(float dt) {
        std::lock_guard<std::mutex> lock(queueMutex);
        
        if (!inputQueue.empty()) {
            for (const auto& input : inputQueue) {
                // If an overlay configuration toggle is on inside mod.json, print transparency logs
                if (Mod::get()->getSettingValue<bool>("show-offset-overlay")) {
                    log::info("Subframe Input Handled accurately at offset: {}s", input.offsetTime);
                }
                
                // Conceptual split step: process fractional segment up to click time
                double segmentA = input.offsetTime;
                if (segmentA > 0.0 && segmentA < dt) {
                    PlayerObject::updatePhysics(segmentA);
                    
                    if (input.isPressed) {
                        this->pushButton(PlayerButton::Jump);
                    } else {
                        this->releaseButton(PlayerButton::Jump);
                    }
                    
                    // Finish out the remaining time segment of this rendering frame
                    double segmentB = dt - segmentA;
                    PlayerObject::updatePhysics(segmentB);
                    continue;
                }
            }
            inputQueue.clear();
        } else {
            PlayerObject::updatePhysics(dt);
        }
    }
};

// Bind registration to Geode configuration systems
$execute {
    log::info("SubFrameDemo custom framework loaded successfully under Alto6755 profile.");
}
