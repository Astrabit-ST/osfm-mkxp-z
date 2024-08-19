#include <SDL3/SDL.h>
#include "etc-internal.h"
#include "eventthread.h"
#include "gl-fun.h"
#include "gl-util.h"
#include "gl-meta.h"
#include "quad.h"
#include "graphics.h"
#include "scene.h"
#include "binding-util.h"
#include "sharedstate.h"

struct PingPong {
    TEXFBO rt[2];
    uint8_t srcInd, dstInd;
    int screenW, screenH;
    
    PingPong(int screenW, int screenH)
    : srcInd(0), dstInd(1), screenW(screenW), screenH(screenH) {
        for (int i = 0; i < 2; ++i) {
            TEXFBO::init(rt[i]);
            TEXFBO::allocEmpty(rt[i], screenW, screenH);
            TEXFBO::linkFBO(rt[i]);
            gl.ClearColor(0, 0, 0, 1);
            FBO::clear();
        }
    }
    
    ~PingPong() {
        for (int i = 0; i < 2; ++i)
            TEXFBO::fini(rt[i]);
    }
    
    TEXFBO &backBuffer() { return rt[srcInd]; }
    
    TEXFBO &frontBuffer() { return rt[dstInd]; }
    
    /* Better not call this during render cycles */
    void resize(int width, int height) {
        screenW = width;
        screenH = height;
        
        for (int i = 0; i < 2; ++i)
            TEXFBO::allocEmpty(rt[i], width, height);
    }
    
    void startRender() { bind(); }
    
    void swapRender() {
        std::swap(srcInd, dstInd);
        
        bind();
    }
    
    void clearBuffers() {
        glState.clearColor.pushSet(Vec4(0, 0, 0, 1));
        
        for (int i = 0; i < 2; ++i) {
            FBO::bind(rt[i].fbo);
            FBO::clear();
        }
        
        glState.clearColor.pop();
    }
    
private:
    void bind() { FBO::bind(rt[dstInd].fbo); }
};

class WindowScene : public Scene {
public:
  PingPong pp;
  Quad screenQuad;

  WindowScene(int w, int h): pp(w, h) {
    geometry.rect.w = w;
    geometry.rect.h = h;

    screenQuad.setTexPosRect(geometry.rect, geometry.rect);
  }

  void composite() {
    glState.viewport.set(IntRect(0, 0, geometry.rect.w, geometry.rect.h));

    pp.startRender();

    gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    FBO::clear();

    Scene::composite();
  }

  void resize(int w, int h) {
    geometry.rect.w = w;
    geometry.rect.h = h;

    pp.resize(w, h);
    screenQuad.setTexPosRect(geometry.rect, geometry.rect);

    notifyGeometryChange();
  }

  void requestViewportRender(const Vec4&c, const Vec4&f, const Vec4&t, const bool s, const Vec4 rx, const Vec4 ry, const float cubic) {
    const IntRect &viewpRect = glState.scissorBox.get();
    const IntRect &screenRect = geometry.rect;

    const bool toneRGBEffect = t.xyzNotNull();
    const bool toneGrayEffect = t.w != 0;
    const bool colorEffect = c.w > 0;
    const bool flashEffect = f.w > 0;
    const bool cubicEffect = cubic != 0;
    const bool rgbOffset = rx.xyzNotNull() || ry.xyzNotNull();
    const bool scannedEffect = s;
            
    if (toneGrayEffect) {
        pp.swapRender();
        
        if (!viewpRect.encloses(screenRect)) {
            /* Scissor test _does_ affect FBO blit operations,
             * and since we're inside the draw cycle, it will
             * be turned on, so turn it off temporarily */
            glState.scissorTest.pushSet(false);
            
            int scaleIsSpecial = GLMeta::blitScaleIsSpecial(pp.frontBuffer(), false, geometry.rect, pp.backBuffer(), geometry.rect);
            GLMeta::blitBegin(pp.frontBuffer(), false, scaleIsSpecial);
            GLMeta::blitSource(pp.backBuffer(), scaleIsSpecial);
            GLMeta::blitRectangle(geometry.rect, Vec2i());
            GLMeta::blitEnd();
            
            glState.scissorTest.pop();
        }
        
        GrayShader &shader = shState->shaders().gray;
        shader.bind();
        shader.setGray(t.w);
        shader.applyViewportProj();
        shader.setTexSize(screenRect.size());
        
        TEX::bind(pp.backBuffer().tex);
        
        glState.blend.pushSet(false);
        screenQuad.draw();
        glState.blend.pop();
    }

    if (scannedEffect)
    {
      pp.swapRender();
      if (!viewpRect.encloses(screenRect))
      {
        /* Scissor test _does_ affect FBO blit operations,
         * and since we're inside the draw cycle, it will
         * be turned on, so turn it off temporarily */
        glState.scissorTest.pushSet(false);
        GLMeta::blitBegin(pp.frontBuffer());
        GLMeta::blitSource(pp.backBuffer());
        GLMeta::blitRectangle(geometry.rect, Vec2i());
        GLMeta::blitEnd();
        glState.scissorTest.pop();
      }
      ScannedShader &shader = shState->shaders().scanned;
      shader.bind();
      shader.applyViewportProj();
      shader.setTexSize(screenRect.size());
      TEX::bind(pp.backBuffer().tex);
      glState.blend.pushSet(false);
      screenQuad.draw();
      glState.blend.pop();
    }

    if (cubicEffect)
    {
      pp.swapRender();
      if (!viewpRect.encloses(screenRect))
      {
        /* Scissor test _does_ affect FBO blit operations,
         * and since we're inside the draw cycle, it will
         * be turned on, so turn it off temporarily */
        glState.scissorTest.pushSet(false);
        GLMeta::blitBegin(pp.frontBuffer());
        GLMeta::blitSource(pp.backBuffer());
        GLMeta::blitRectangle(geometry.rect, Vec2i());
        GLMeta::blitEnd();
        glState.scissorTest.pop();
      }
      CubicShader &shader = shState->shaders().cubic;
      shader.bind();
      shader.setiTime(cubic);
      shader.applyViewportProj();
      shader.setTexSize(screenRect.size());
      TEX::bind(pp.backBuffer().tex);
      glState.blend.pushSet(false);
      screenQuad.draw();
      glState.blend.pop();
    }
    if (rgbOffset)
    {
      pp.swapRender();
      if (!viewpRect.encloses(screenRect))
      {
        /* Scissor test _does_ affect FBO blit operations,
         * and since we're inside the draw cycle, it will
         * be turned on, so turn it off temporarily */
        glState.scissorTest.pushSet(false);
        GLMeta::blitBegin(pp.frontBuffer());
        GLMeta::blitSource(pp.backBuffer());
        GLMeta::blitRectangle(geometry.rect, Vec2i());
        GLMeta::blitEnd();
        glState.scissorTest.pop();
      }
      ChronosShader &shader = shState->shaders().chronos;
      shader.bind();
      shader.setrgbOffset(rx, ry);
      shader.applyViewportProj();
      shader.setTexSize(screenRect.size());
      TEX::bind(pp.backBuffer().tex);
      glState.blend.pushSet(false);
      screenQuad.draw();
      glState.blend.pop();
    }

    if (!toneRGBEffect && !colorEffect && !flashEffect)
        return;

    FlatColorShader &shader = shState->shaders().flatColor;
    shader.bind();
    shader.applyViewportProj();
            
    if (toneRGBEffect) {
        /* First split up additive / substractive components */
        Vec4 add, sub;
        
        if (t.x > 0)
            add.x = t.x;
        if (t.y > 0)
            add.y = t.y;
        if (t.z > 0)
            add.z = t.z;
          
        if (t.x < 0)
            sub.x = -t.x;
        if (t.y < 0)
            sub.y = -t.y;
        if (t.z < 0)
            sub.z = -t.z;
          
        /* Then apply them using hardware blending */
        gl.BlendFuncSeparate(GL_ONE, GL_ONE, GL_ZERO, GL_ONE);
        
        if (add.xyzNotNull()) {
            gl.BlendEquation(GL_FUNC_ADD);
            shader.setColor(add);
            
            screenQuad.draw();
        }
        
        if (sub.xyzNotNull()) {
            gl.BlendEquation(GL_FUNC_REVERSE_SUBTRACT);
            shader.setColor(sub);
            
            screenQuad.draw();
        }
    }

    if (colorEffect || flashEffect) {
        gl.BlendEquation(GL_FUNC_ADD);
        gl.BlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ZERO,
                             GL_ONE);
    }

    if (colorEffect) {
        shader.setColor(c);
        screenQuad.draw();
    }

    if (flashEffect) {
        shader.setColor(f);
        screenQuad.draw();
    }

    glState.blendMode.refresh();
  }
};

struct MonitorWindow {
  SDL_Window* window;
  WindowScene scene;

  MonitorWindow(int x, int y, int w, int h, unsigned int flags, const char* name) : scene(w, h) {
    EventThread::CreateWindowArgs args = {x, y, w, h, flags, name};
    window = shState->eThread().requestNewWindow(&args);
    shState->monitorWindows.insert(this);
    render();
  }
  ~MonitorWindow() {
    shState->monitorWindows.erase(this);
    shState->eThread().destroySDLWindow(window); // do it on the event thread
  }

  // renders to ping pong buffer
  void render();
  Scene* getScene();
};
// so we can use this from outside this file
Scene* MonitorWindow::getScene() {
  return &scene;
}
void MonitorWindow::render() {
  SDL_GLContext ctx = shState->graphics().context();
  SDL_GL_MakeCurrent(window, ctx);
  scene.composite();
  auto geo = scene.getGeometry();
  int w = geo.rect.w;
  int h = geo.rect.h;

  GLMeta::blitBeginScreen(Vec2i(w ,h), false);
  GLMeta::blitSource(scene.pp.frontBuffer(), 0);

  gl.ClearColor(0.0f, 0.0f, 0.0f, 0.0f);
  FBO::clear();

  GLMeta::blitRectangle(IntRect(0, 0, w, h), IntRect(0,h,w,-h), false);

  GLMeta::blitEnd();
  SDL_GL_SwapWindow(window); 
}

DEF_TYPE(MonitorWindow);

#define GUARD_DISPOSED(w) if (!w->window) rb_raise(rb_eRuntimeError, "Window already disposed!");

RB_METHOD(monitorWindowInit) {
  VALUE vx, vy, vw, vh;
  VALUE kwargs;
  rb_scan_args(argc, argv, "4:", &vx, &vy, &vw, &vh, &kwargs);

  int x = NUM2INT(vx);
  int y = NUM2INT(vy);
  int w = NUM2INT(vw);
  int h = NUM2INT(vh);

  if (w < 1 || h < 1)
    rb_raise(rb_eArgError, "Invalid window size");

  const char* name = " ";
  unsigned int flags = SDL_WINDOW_OPENGL | SDL_WINDOW_UTILITY | SDL_WINDOW_BORDERLESS | SDL_WINDOW_TRANSPARENT;
  if (!NIL_P(kwargs)) {
    ID table[7] = {
      rb_intern("borderless"),
      rb_intern("hidden"),
      rb_intern("always_on_top"),
      rb_intern("fullscreen"),
      rb_intern("skip_taskbar"),
      rb_intern("transparent"),
      rb_intern("name"),
    };
    VALUE values[7];

    rb_get_kwargs(kwargs, table, 0, 7, values);

    if (!RTEST(values[0]) && values[0] != Qundef)
      flags &= ~SDL_WINDOW_BORDERLESS; // enabled by default
    if (RTEST(values[1]) && values[1] != Qundef)
      flags |= SDL_WINDOW_HIDDEN; // shown by default
    if (RTEST(values[2]) && values[2] != Qundef)
      flags |= SDL_WINDOW_ALWAYS_ON_TOP; // not always on top by default
    if (RTEST(values[3]) && values[3] != Qundef)
      flags |= SDL_WINDOW_FULLSCREEN; // not fullscreen by default
    if (!RTEST(values[4]) && values[4] != Qundef)
      flags &= ~SDL_WINDOW_UTILITY; // skipped by default
    if (!RTEST(values[5]) && values[5] != Qundef)
      flags &= ~SDL_WINDOW_TRANSPARENT; // not transparent by default
    if (values[6] != Qundef)
      name = StringValueCStr(values[6]);
  }

  MonitorWindow* window = new MonitorWindow(x, y, w, h, flags, name);

  setPrivateData(self, window);

  return self;
}

RB_METHOD(monitorWindowDispose) {
  MonitorWindow* w = getPrivateData<MonitorWindow>(self);

  delete w;
  setPrivateData(self, nullptr);

  return Qnil;
}

RB_METHOD(monitorWindowResize) {
  MonitorWindow* window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  int w, h;
  rb_get_args(argc, argv, "ii", &w, &h RB_ARG_END);

  if (w < 1 || h < 1)
    rb_raise(rb_eArgError, "Invalid window size");

  SDL_SetWindowSize(window->window, w, h);
  window->scene.resize(w, h);

  return Qnil;
}

RB_METHOD(monitorWindowMove) {
  MonitorWindow* window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  int x, y;
  rb_get_args(argc, argv, "ii", &x, &y RB_ARG_END);

  SDL_SetWindowPosition(window->window, x, y);

  return Qnil;
}

RB_METHOD(monitorWindowPos) {
  MonitorWindow* window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  int x, y;
  SDL_GetWindowPosition(window->window, &x, &y);

  return rb_ary_new_from_args(2, INT2NUM(x), INT2NUM(y));
}

RB_METHOD(monitorWindowSize) {
  MonitorWindow* window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  int w, h;
  SDL_GetWindowSize(window->window, &w, &h);

  return rb_ary_new_from_args(2, INT2NUM(w), INT2NUM(h));
}

RB_METHOD(monitorWindowShow) {
  MonitorWindow* window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  shState->eThread().requestWindowVisible(window->window, true);

  return Qnil;
}

RB_METHOD(monitorWindowHide) {
  MonitorWindow* window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

  shState->eThread().requestWindowVisible(window->window, false);

  return Qnil;
}

RB_METHOD(monitorSetAlwaysOnTop) {
  MonitorWindow* window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

	bool top;
	rb_get_args(argc, argv, "b", &top);

	SDL_SetWindowAlwaysOnTop(window->window, top);

	return Qnil;
}

RB_METHOD(monitorFlashWindow) {
  MonitorWindow* window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

	int state;
	rb_get_args(argc, argv, "i", &state);

	if (state < SDL_FLASH_CANCEL || state > SDL_FLASH_UNTIL_FOCUSED)
		rb_raise(rb_eArgError, "Invalid flash state");

	SDL_FlashWindow(window->window, (SDL_FlashOperation) state);
	return Qnil;
}

RB_METHOD(monitorWindowRaise) {
  MonitorWindow* window = getPrivateData<MonitorWindow>(self);

  GUARD_DISPOSED(window);

	SDL_RaiseWindow(window->window);

	return Qnil;
}

void osfmBindingInit() {

  VALUE klass = rb_define_class("MonitorWindow", rb_cObject);
  rb_define_alloc_func(klass, classAllocate<&MonitorWindowType>);

  _rb_define_method(klass, "initialize", monitorWindowInit);
  _rb_define_method(klass, "dispose", monitorWindowDispose);
  _rb_define_method(klass, "size", monitorWindowSize);
  _rb_define_method(klass, "resize", monitorWindowResize);
  _rb_define_method(klass, "move_to", monitorWindowMove);
  _rb_define_method(klass, "position", monitorWindowPos);
  _rb_define_method(klass, "show", monitorWindowShow);
  _rb_define_method(klass, "hide", monitorWindowHide);
  _rb_define_method(klass, "set_always_on_top", monitorSetAlwaysOnTop);
  _rb_define_method(klass, "flash", monitorFlashWindow);
  _rb_define_method(klass, "raise", monitorWindowRaise);

  rb_define_const(klass, "UNDEFINED_POS", INT2NUM(SDL_WINDOWPOS_UNDEFINED));
  rb_define_const(klass, "CENTERED_POS", INT2NUM(SDL_WINDOWPOS_CENTERED));

}