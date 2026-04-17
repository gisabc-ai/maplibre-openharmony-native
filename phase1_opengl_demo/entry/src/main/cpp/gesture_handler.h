#ifndef GESTURE_HANDLER_H
#define GESTURE_HANDLER_H

#include <cstdint>

namespace maplibre {

/**
 * GestureHandler — 手势识别器
 * 
 * 支持识别：
 *   - TAP:        单指快速点击
 *   - DOUBLE_TAP: 双击（放大）
 *   - PAN:        单指拖拽平移
 *   - PINCH:      双指捏合缩放
 *   - NONE:       无有效手势
 * 
 * 使用方法：
 *   每帧从输入事件获取 dx/dy/scale，调用 detect()，返回手势类型。
 */
class GestureHandler {
public:
    enum Gesture {
        NONE = 0,
        TAP,
        DOUBLE_TAP,
        PAN,
        PINCH
    };

    GestureHandler();

    /**
     * 检测手势
     * 
     * @param dx        本帧 X 方向像素位移（正值=向右）
     * @param dy        本帧 Y 方向像素位移（正值=向下）
     * @param scale     本帧缩放比例（1.0=无变化，>1=放大，<1=缩小）
     * @param fingers   当前触控手指数量（0/1/2）
     * @return          识别到的手势类型
     */
    Gesture detect(float dx, float dy, float scale, int fingers);

    /**
     * 重置手势状态
     * 在手势事件序列开始时调用（如手指按下）
     */
    void reset();

    /**
     * 获取上次检测到的平移量（像素）
     */
    void getLastPan(float* outDx, float* outDy) const;

    /**
     * 获取上次检测到的缩放量
     * @return 缩放比例（相对手势开始时）
     */
    float getLastScale() const { return scaleBase_; }

    /**
     * 判断是否需要重绘（手势进行中）
     */
    bool isActive() const { return gesture_ != NONE; }

private:
    Gesture gesture_;
    int fingers_;

    // 点击检测
    float tapStartX_;
    float tapStartY_;
    uint64_t tapStartTime_;
    bool waitingSecondTap_;

    // 双击检测
    uint64_t lastTapTime_;
    float lastTapX_;
    float lastTapY_;

    // PAN
    float lastPanX_;
    float lastPanY_;
    float totalPanX_;
    float totalPanY_;

    // PINCH
    float scaleBase_;
    float lastScale_;

    static constexpr uint64_t TAP_TIMEOUT_MS = 300;
    static constexpr uint64_t DOUBLE_TAP_TIMEOUT_MS = 300;
    static constexpr float TAP_MOVE_THRESHOLD = 15.0f;
    static constexpr float DOUBLE_TAP_MOVE_THRESHOLD = 30.0f;
    static constexpr float PINCH_SCALE_THRESHOLD = 0.01f;
};

} // namespace maplibre

#endif // GESTURE_HANDLER_H
