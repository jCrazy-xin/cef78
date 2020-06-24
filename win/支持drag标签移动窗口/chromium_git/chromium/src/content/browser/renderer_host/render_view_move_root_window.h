#ifndef RENDER_VIEW_MOVE_ROOT_WINDOW  
#define RENDER_VIEW_MOVE_ROOT_WINDOW  
  
namespace content {
class RenderViewMoveRootWindow {
 public:
  RenderViewMoveRootWindow() {}
  virtual ~RenderViewMoveRootWindow() {}
  virtual void SetMoveWindow(bool move_enabled) = 0;
};
}  // namespace content

#endif // RENDER_VIEW_MOVE_ROOT_WINDOW  