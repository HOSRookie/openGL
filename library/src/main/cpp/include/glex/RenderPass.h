#pragma once

/**
 * @file RenderPass.h
 * @brief 娓叉煋闃舵鎶借薄鍩虹被
 *
 * 瀹氫箟浜嗘覆鏌撶绾夸腑鍗曚釜 Pass 鐨勬帴鍙ｃ€?
 * 寮€鍙戣€呯户鎵挎绫诲疄鐜拌嚜瀹氫箟鐨勬覆鏌撻€昏緫銆?
 *
 * 鐢ㄦ硶锛?
 *   class MySkyPass : public RenderPass {
 *   public:
 *       MySkyPass() : RenderPass("SkyPass") {}
 *       void onInitialize(int w, int h) override { ... }
 *       void onUpdate(float dt) override { ... }
 *       void onRender() override { ... }
 *       void onDestroy() override { ... }
 *   };
 */

#include <string>

namespace glex {

class RenderPass {
public:
    explicit RenderPass(const std::string& name) : name_(name) {}
    virtual ~RenderPass() = default;

    // 绂佹鎷疯礉
    RenderPass(const RenderPass&) = delete;
    RenderPass& operator=(const RenderPass&) = delete;

    /** 鍒濆鍖栵紙GL 涓婁笅鏂囧凡灏辩华鏃惰皟鐢級 */
    void initialize(int width, int height) {
        width_ = width;
        height_ = height;
        onInitialize(width, height);
        initialized_ = true;
    }

    /** 灏哄鍙樺寲 */
    void resize(int width, int height) {
        width_ = width;
        height_ = height;
        onResize(width, height);
    }

    /** 姣忓抚鏇存柊閫昏緫 */
    void update(float deltaTime) {
        if (enabled_ && initialized_) {
            onUpdate(deltaTime);
        }
    }

    // Render
    void render() {
        if (enabled_ && initialized_) {
            onRender();
        }
    }

    // Touch event
    void touch(float x, float y, int action, int pointerId) {
        if (enabled_ && initialized_) {
            onTouch(x, y, action, pointerId);
        }
    }

    /** 閿€姣佽祫婧?*/
    void destroy() {
        if (initialized_) {
            onDestroy();
            initialized_ = false;
        }
    }

    // ============================================================
    // 灞炴€?
    // ============================================================

    const std::string& getName() const { return name_; }

    bool isEnabled() const { return enabled_; }
    void setEnabled(bool enabled) { enabled_ = enabled; }

    bool isInitialized() const { return initialized_; }

    int getWidth() const { return width_; }
    int getHeight() const { return height_; }

protected:
    /** 瀛愮被瀹炵幇锛氬垵濮嬪寲 GL 璧勬簮 */
    virtual void onInitialize(int width, int height) = 0;

    /** 瀛愮被瀹炵幇锛氬昂瀵稿彉鍖栧鐞?*/
    virtual void onResize(int width, int height) {}

    /** 瀛愮被瀹炵幇锛氭瘡甯ф洿鏂?*/
    virtual void onUpdate(float deltaTime) {}

    /** 瀛愮被瀹炵幇锛氭覆鏌撶粯鍒?*/
    virtual void onRender() = 0;

    virtual void onTouch(float x, float y, int action, int pointerId) {
        (void)x;
        (void)y;
        (void)action;
        (void)pointerId;
    }

    /** 瀛愮被瀹炵幇锛氶攢姣?GL 璧勬簮 */
    virtual void onDestroy() = 0;

    std::string name_;
    bool enabled_ = true;
    bool initialized_ = false;
    int width_ = 0;
    int height_ = 0;
};

} // namespace glex
