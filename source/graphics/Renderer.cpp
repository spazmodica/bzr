#include "graphics/Renderer.h"
#include "graphics/LandblockRenderer.h"
#include "graphics/SkyRenderer.h"
#include "graphics/util.h"
#include "math/Mat3.h"
#include "Camera.h"
#include "Core.h"
#include "Landblock.h"
#include "util.h"
#ifdef OCULUSVR
#include <SDL_syswm.h>
#include <algorithm>
#endif

Renderer::Renderer() : _videoInit(false), _window(nullptr), _context(nullptr)
#ifdef OCULUSVR
    , _hmd(nullptr), _renderTex(0), _depthTex(0), _framebuffer(0)
#endif
{
    if(SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
    {
        throwSDLError();
    }

    _videoInit = true;

#ifdef __APPLE__
    // Apple's drivers don't support the compatibility profile on GL >v2.1
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
#endif

    // Enable 16x MSAA
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 16);

    // TODO configurable
    _width = 1024;
    _height = 768;
    _fieldOfView = 90.0;

    _window = SDL_CreateWindow("Bael'Zharon's Respite",
        SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, _width, _height, SDL_WINDOW_OPENGL);

    if(_window == nullptr)
    {
        throwSDLError();
    }

    _context = SDL_GL_CreateContext(_window);

    if(_context == nullptr)
    {
        throwSDLError();
    }

#ifdef _MSC_VER
	auto glewErr = glewInit();

	if(glewErr != GLEW_OK)
	{
        string err("Unable to initialize GLEW: ");
        err.append((const char*)glewGetErrorString(glewErr));
        throw runtime_error(err);
	}
#endif

#ifdef OCULUSVR
    ovr_Initialize();

    _hmd = ovrHmd_Create(0);

    if(!_hmd)
    {
        throw runtime_error("Failed to create OVR HMD object");
    }

    auto leftEyeTexSize = ovrHmd_GetFovTextureSize(_hmd, ovrEye_Left, _hmd->DefaultEyeFov[ovrEye_Left], 1.0f);
    auto rightEyeTexSize = ovrHmd_GetFovTextureSize(_hmd, ovrEye_Right, _hmd->DefaultEyeFov[ovrEye_Right], 1.0f);

    _renderTexSize.w = leftEyeTexSize.w + rightEyeTexSize.w;
    _renderTexSize.h = max(leftEyeTexSize.h, rightEyeTexSize.h);

    glGenTextures(1, &_renderTex);
    glBindTexture(GL_TEXTURE_2D, _renderTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, _renderTexSize.w, _renderTexSize.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &_renderTexSize.w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &_renderTexSize.h);

    glGenTextures(1, &_depthTex);
    glBindTexture(GL_TEXTURE_2D, _depthTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, _renderTexSize.w, _renderTexSize.h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_BYTE, nullptr);

    _eyeViewport[ovrEye_Left] = { { 0, 0 }, { _renderTexSize.w / 2, _renderTexSize.h } };
    _eyeViewport[ovrEye_Right] = { { _renderTexSize.w / 2, 0 }, { _renderTexSize.w / 2, _renderTexSize.h } };

    _eyeTexture[ovrEye_Left].OGL.Header.API = ovrRenderAPI_OpenGL;
    _eyeTexture[ovrEye_Left].OGL.Header.TextureSize = _renderTexSize;
    _eyeTexture[ovrEye_Left].OGL.Header.RenderViewport = _eyeViewport[ovrEye_Left];
    _eyeTexture[ovrEye_Left].OGL.TexId = _renderTex;

    _eyeTexture[ovrEye_Right].OGL.Header.API = ovrRenderAPI_OpenGL;
    _eyeTexture[ovrEye_Right].OGL.Header.TextureSize = _renderTexSize;
    _eyeTexture[ovrEye_Right].OGL.Header.RenderViewport = _eyeViewport[ovrEye_Right];
    _eyeTexture[ovrEye_Right].OGL.TexId = _renderTex;

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);

    if(!SDL_GetWindowWMInfo(_window, &wmInfo))
    {
        throw runtime_error("Failed to get window info");
    }

    ovrGLConfig cfg;
    cfg.OGL.Header.API = ovrRenderAPI_OpenGL;
    cfg.OGL.Header.RTSize = _renderTexSize;
    cfg.OGL.Header.Multisample = 1; // yes?
    cfg.OGL.Window = wmInfo.info.win.window;
    cfg.OGL.DC = GetDC(wmInfo.info.win.window);

    unsigned int distortionCaps = 0;
    ovrEyeRenderDesc eyeRenderDesc[ovrEye_Count];

    if(!ovrHmd_AttachToWindow(_hmd, wmInfo.info.win.window, nullptr, nullptr))
    {
        throw runtime_error("Failed to attach OVR to window");
    }

    if(!ovrHmd_ConfigureRendering(_hmd, &cfg.Config, distortionCaps, _hmd->DefaultEyeFov, eyeRenderDesc))
    {
        throw runtime_error("Failed to configure OVR rendering");
    }

    glGenFramebuffers(1, &_framebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, _renderTex, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, _depthTex, 0);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        throw runtime_error("Framebuffer not complete");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
#endif

    SDL_GL_SetSwapInterval(1); // vsync
    glEnable(GL_DEPTH_TEST);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    //glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1); // the default is 4

    _skyRenderer.reset(new SkyRenderer());
    _landblockRenderer.reset(new LandblockRenderer());
    _landblockRenderer->setLightPosition(_skyRenderer->sunVector() * 1000.0);
}

Renderer::~Renderer()
{
    _landblockRenderer.reset();
    _skyRenderer.reset();

#ifdef OCULUSVR
    glDeleteFramebuffers(1, &_framebuffer);
    glDeleteTextures(1, &_renderTex);
    glDeleteTextures(1, &_depthTex);

    ovrHmd_Destroy(_hmd);
    ovr_Shutdown();
#endif

    if(_context != nullptr)
    {
        SDL_GL_DeleteContext(_context);
    }

    if(_window != nullptr)
    {
        SDL_DestroyWindow(_window);
    }

    if(_videoInit)
    {
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }
}

void Renderer::render(double interp)
{
    (void)interp;

#ifdef OCULUSVR
    // shhhhh...
    ovrHmd_DismissHSWDisplay(_hmd);

    ovrHmd_BeginFrame(_hmd, 0);

    ovrPosef eyePose[ovrEye_Count];

    glBindFramebuffer(GL_FRAMEBUFFER, _framebuffer);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    for(auto i = 0; i < ovrEye_Count; i++)
    {
        auto eye = _hmd->EyeRenderOrder[i];

        glViewport(_eyeViewport[eye].Pos.x, _eyeViewport[eye].Pos.y,
                   _eyeViewport[eye].Size.w, _eyeViewport[eye].Size.h);

        eyePose[eye] = ovrHmd_GetEyePose(_hmd, eye);

        Mat4 projectionMat;
        projectionMat.makePerspective(_fieldOfView, double(_width)/double(_height), 0.1, 1000.0);

        const Mat4& viewMat = Core::get().camera().viewMatrix();

        _skyRenderer->render();
        _landblockRenderer->render(projectionMat, viewMat);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    ovrHmd_EndFrame(_hmd, eyePose, (ovrTexture*)_eyeTexture);
#else
    // projection * view * model * vertex
    Mat4 projectionMat;
    projectionMat.makePerspective(_fieldOfView, double(_width)/double(_height), 0.1, 1000.0);

    const Mat4& viewMat = Core::get().camera().viewMatrix();

    glClear(GL_DEPTH_BUFFER_BIT);

    _skyRenderer->render();
    _landblockRenderer->render(projectionMat, viewMat);

    SDL_GL_SwapWindow(_window);
#endif
}
