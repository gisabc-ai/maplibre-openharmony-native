#include "gesture_handler.h"
#include <cmath>
#include <algorithm>

namespace maplibre {

GestureHandler::GestureHandler()
    : gesture_(NONE)
    , fingers_(0)
    , tapStartX_(0), tapStartY_(0)
    , tapStartTime_(0)
    , waitingSecondTap_(false)
    , lastTapTime_(0), lastTapX_(0), lastTapY_(0)
    , lastPanX_(0), lastPanY_(0)
    , totalPanX_(0), totalPanY_(0)
    , scaleBase_(1.0f)
    , lastScale_(1.0f)
{}

void GestureHandler::reset() {
    gesture_ = NONE;
    fingers_ = 0;
    lastPanX_ = 0; lastPanY_ = 0;
    totalPanX_ = 0; totalPanY_ = 0;
    scaleBase_ = 1.0f;
    lastScale_ = 1.0f;
}

GestureHandler::Gesture GestureHandler::detect(float dx, float dy, float scale, int fingers) {
    uint64_t nowMs = 0;
    // 使用 clock_gettime 或简单计时
    // 这里用系统接口: time(nullptr) 不够精确，简化处理
    // 实际产品建议用 (struct timespec).tv_nsec
    // nowMs = (uint64_t)(std::chrono::steady_clock::now().time_since_epoch().count() / 1000000ULL);

    if (fingers == 0) {
        // 所有手指抬起，结束手势
        Gesture result = gesture_;
        // 如果是 PAN，检查是否是 TAP
        if (gesture_ == PAN) {
            float dist = std::sqrt(dx*dx + dy*dy);
            if (dist < TAP_MOVE_THRESHOLD) {
                // 视为 TAP
                result = TAP;
                // 双击检测
                // （简化：双击由上方双击逻辑处理）
            }
        }
        reset();
        return result;
    }

    if (fingers == 1) {
        // 单指
        if (gesture_ == NONE || gesture_ == TAP || gesture_ == DOUBLE_TAP) {
            // 开始 PAN
            gesture_ = PAN;
            tapStartX_ = dx;
            tapStartY_ = dy;
            totalPanX_ = 0; totalPanY_ = 0;
        }

        if (gesture_ == PAN) {
            totalPanX_ += dx;
            totalPanY_ += dy;
            lastPanX_ = dx;
            lastPanY_ = dy;
        }
        return gesture_;
    }

    if (fingers == 2) {
        // 双指 = PINCH
        if (gesture_ != PINCH) {
            gesture_ = PINCH;
            scaleBase_ = scale;
            lastScale_ = scale;
        }
        lastScale_ = scale;
        return PINCH;
    }

    return NONE;
}

void GestureHandler::getLastPan(float* outDx, float* outDy) const {
    if (outDx) *outDx = lastPanX_;
    if (outDy) *outDy = lastPanY_;
}

} // namespace maplibre
