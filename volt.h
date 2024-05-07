#pragma once
// #ifndef VOLT_H
// #define VOLT_H
#include <iostream>
#include <string_view>
#include <cmath>
#include <vector>
#include <array>
#include <deque>
#include <stack>
#include <list>
#include <string>
#include <span>
#include <chrono>
#include <algorithm>
#include <memory>
#include <future>
#include <random>
#include <optional>
#include <execution>
#include <stdexcept>
#include <unordered_map>
#include <map>
#include <unordered_set>
#include <set>
#include <type_traits>
#include <any>
#include <typeindex>
#include <typeinfo>
#include <filesystem>
#include <functional>
#include <sstream>
#include <atomic>
#include <mutex>
#include <SDL3/SDL.h>
#include <SDL3/SDL_image.h>
#include <SDL3/SDL_ttf.h>
#include "volt_util.h"
#include "volt_fonts.h"
#ifdef _WIN32
#include <windows.h>
#endif

static float tB_sP = 0.f, sn_spacing = 0.f;

namespace Volt
{
	SDL_Color WHITE = {0xFF, 0xFF, 0xFF, 0xFF};
	SDL_Color BLACK = {0x00, 0x00, 0x00, 0xff};
	SDL_Color YELLOW = {0xff, 0xff, 0x00, 0xff};
	SDL_Color BLUE = {0x00, 0x00, 0xff, 0xff};
	SDL_Color BLANK = {0x00, 0x00, 0x00, 0x00};

	SDL_BlendMode blendMode = SDL_BLENDMODE_BLEND;

	/*
	 * notice the use of std::mutext instead of using std::atomic
	 * this is because class CellBlock relies on AdaptiveVsync alot but CellBlock must be copy constructible
	 * and std::atomic's are'nt;
	 */
	std::mutex AdaptiveVsyncMux;

	class AdaptiveVsyncHandler;

	class AdaptiveVsync
	{
	public:
		friend class AdaptiveVsyncHandler;

		auto pollEvent(SDL_Event *_event) const -> int
		{
			if (no_sleep_requests)
				return SDL_PollEvent(_event);
			else
				return SDL_WaitEvent(_event);
		}

		auto hasRequests()
		{
			if (!no_sleep_requests)
				return false;
			return true;
		}

	private:
		std::uint32_t no_sleep_requests = 0;
	};

	/*
	Volt is an event driven sys and only draws upon request("only when necessary").
	if you have an object/drawable that needs the drawing thread to stay alive eg animation, you must use this class to
	tell volt when not to hybernate the drawing thread
	*/
	class AdaptiveVsyncHandler
	{
	public:
		void setAdaptiveVsync(AdaptiveVsync *_adaptiveVsync)
		{
			adaptiveVsync = _adaptiveVsync;
		}

		void startRedrawSession()
		{
			if (!redrawRequested)
			{
				std::scoped_lock lock(AdaptiveVsyncMux);
				adaptiveVsync->no_sleep_requests++;
				redrawRequested = true;
			}
		}

		void stopRedrawSession()
		{
			if (redrawRequested)
			{
				std::scoped_lock lock(AdaptiveVsyncMux);
				adaptiveVsync->no_sleep_requests--;
				redrawRequested = false;
			}
		}

		bool shouldReDrawFrame()
		{
			return redrawRequested;
		}

	private:
		bool redrawRequested = false;
		AdaptiveVsync *adaptiveVsync = nullptr;
	};

	class CacheRenderTarget
	{
	public:
		CacheRenderTarget(SDL_Renderer *_renderer) : targetCache(SDL_GetRenderTarget(_renderer)) {}
		CacheRenderTarget(const CacheRenderTarget &) = delete;
		CacheRenderTarget(const CacheRenderTarget &&) = delete;
		void cache(SDL_Renderer *_renderer)
		{
			targetCache = SDL_GetRenderTarget(_renderer);
		}
		void release(SDL_Renderer *_renderer)
		{
			SDL_SetRenderTarget(_renderer, targetCache);
		}

	private:
		SDL_Texture *targetCache;
	};

	SDL_Color CACHE_COLOR;
	auto CacheRenderColor = [](SDL_Renderer *renderer)
	{
		SDL_GetRenderDrawColor(renderer, &CACHE_COLOR.r, &CACHE_COLOR.g, &CACHE_COLOR.b,
							   &CACHE_COLOR.a);
	};

	auto RestoreCachedRenderColor = [](SDL_Renderer *renderer)
	{
		SDL_SetRenderDrawColor(renderer, CACHE_COLOR.r, CACHE_COLOR.g, CACHE_COLOR.b,
							   CACHE_COLOR.a);
	};

	class ScopeTimer
	{
	public:
		ScopeTimer(const char *name) : m_name(name)
		{
			m_start = std::chrono::high_resolution_clock::now();
		}

		~ScopeTimer()
		{
			std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() -
												m_start);
			SDL_Log("ScopeTimer_ID: %s: %f secs", m_name.c_str(), dt.count());
		}

		ScopeTimer(const ScopeTimer &) = delete;

		ScopeTimer(const ScopeTimer &&) = delete;

	private:
		std::string m_name;
		std::chrono::steady_clock::time_point m_start;
	};

	/// TODO: windows&renderer id's class or smn'
	struct ResourceDeleter
	{
		void operator()(SDL_Window *window_) const
		{
			if (window_ != nullptr)
				SDL_DestroyWindow(window_);
			window_ = nullptr;
			SDL_Log("Window Destroyed!");
		}

		void operator()(SDL_Renderer *renderer_) const
		{
			if (renderer_ != nullptr)
				SDL_DestroyRenderer(renderer_);
			renderer_ = nullptr;
			SDL_Log("Renderer Destroyed!");
		}

		void operator()(SDL_Texture *texture_) const
		{
			if (texture_ != nullptr)
			{
				SDL_DestroyTexture(texture_);
				texture_ = nullptr;
				// SDL_Log("Texture Destroyed!");
			}
		}
	};

	using UniqueTexture = std::unique_ptr<SDL_Texture, ResourceDeleter>;
	using SharedTexture = std::shared_ptr<SDL_Texture>;

	std::unique_ptr<SDL_Window, ResourceDeleter>
	CreateUniqueWindow(const char *title, Width<int> w,
					   Height<int> h, uint32_t flags)
	{
		return std::unique_ptr<SDL_Window, ResourceDeleter>(
			SDL_CreateWindow(title, w.get(), h.get(), flags), ResourceDeleter());
	}

	std::shared_ptr<SDL_Window>
	CreateSharedWindow(const char *title, int w, int h, uint32_t flags)
	{
		return std::shared_ptr<SDL_Window>(
			SDL_CreateWindow(title, w, h, flags), ResourceDeleter());
	}

	std::unique_ptr<SDL_Renderer, ResourceDeleter>
	CreateUniqueRenderer(SDL_Window *window, int index, uint32_t flags)
	{
		return std::unique_ptr<SDL_Renderer, ResourceDeleter>(
			SDL_CreateRenderer(window, NULL, flags), ResourceDeleter());
	}

	std::shared_ptr<SDL_Renderer>
	CreateSharedRenderer(SDL_Window *window, int index, uint32_t flags)
	{
		return std::shared_ptr<SDL_Renderer>(
			SDL_CreateRenderer(window, NULL, flags), ResourceDeleter());
	}

	inline UniqueTexture
	CreateUniqueTexture(SDL_Renderer *renderer, Uint32 format, int access, const int w,
						const int h)
	{
		return UniqueTexture(
			SDL_CreateTexture(renderer, format, access, w, h),
			ResourceDeleter());
	}

	SharedTexture
	CreateSharedTexture(SDL_Renderer *renderer, Uint32 format, int access, const int &w,
						const int &h)
	{
		return SharedTexture(
			SDL_CreateTexture(renderer, format, access, w, h),
			ResourceDeleter());
	}

	SharedTexture CreateSharedTextureFromSurface(SDL_Renderer *renderer, SDL_Surface *surface)
	{
		return SharedTexture(
			SDL_CreateTextureFromSurface(renderer, surface),
			ResourceDeleter());
	}

	SharedTexture LoadSharedTexture(SDL_Renderer *renderer, const std::string &_img_path)
	{
		return SharedTexture(
			IMG_LoadTexture(renderer, _img_path.c_str()),
			ResourceDeleter());
	}

	std::unique_ptr<SDL_Texture, ResourceDeleter>
	CreateUniqueTextureFromSurface(SDL_Renderer *renderer, SDL_Surface *surface)
	{
		return std::unique_ptr<SDL_Texture, ResourceDeleter>(SDL_CreateTextureFromSurface(renderer, surface));
	}

	auto DestroyTextureSafe = [](SDL_Texture *texture)
	{
		if (texture != nullptr)
		{
			SDL_DestroyTexture(texture);
			texture = nullptr;
		}
	};

	/*
	 * Display Metrics
	 */
	// class DisplayMetrics {
	// public:
	//	DisplayMetrics() {
	//		display_type = DeviceDisplayType::UNKOWN;
	//		DrawableH = 0, DrawableW = 0;
	//	    RenderH = RenderW = DPRenderH = DPRenderW = DDPI = H_DPI = V_DPI = V_DPI_R = H_DPI_R = 0;
	//	}
	//	float RenderH, RenderW, DPRenderH, DPRenderW, DDPI, H_DPI, V_DPI, V_DPI_R, H_DPI_R;
	//	int DrawableH, DrawableW;
	//	SDL_DisplayMode mode;
	//	DeviceDisplayType display_type;

	//	//float x, y, w, h;
	//	//convert %val to Height
	//	template<typename T>
	//	constexpr inline T toH(const T& val) const { return ((val * RenderH) / static_cast<decltype(val)>(100)); }

	//	//convert %val to width
	//	template<typename T>
	//	constexpr inline T toW(const T& val) const {
	//		return ((val * RenderW) / static_cast<decltype(val)>(100));
	//	}

	//	template<typename T>
	//	constexpr inline T dpToPx(const T& dp) const {
	//		return (DDPI * dp) / 160.f;
	//	}

	//	//convert %val to width
	//	template<typename T>
	//	constexpr inline T to_w_dp(const T& val) const {
	//		return ((val * DPRenderW) / 100);
	//	}

	//	//convert %val to width
	//	template<typename T>
	//	constexpr inline T to_h_dp(const T& val) const {
	//		return ((val * DPRenderH) / 100);
	//	}

	//	template<typename T>
	//	constexpr inline T to_min(const T& val, const T& a, const T& b) {
	//		if (a < b || a == b) {
	//			return toCust(val, a);
	//			//return ((val * a) / 100);
	//		}
	//		else if (a > b) {
	//			return toCust(val, b);
	//			//return ((val * b) / 100);
	//		}
	//	}

	//	template<typename T>
	//	constexpr inline T toCust(const T& val, const T& ref) const {
	//		return ((val * ref) / static_cast<decltype(val)>(100));
	//	}

	//	void initDisplaySystem(const float& display_index = 0.f) {
	//		SDL_GetDisplayDPI(display_index, &DDPI, &H_DPI, &V_DPI);
	//	}

	//	inline const std::pair<int, int> getDeviceSize() noexcept {
	//		SDL_DisplayMode tmpMode;
	//		SDL_GetDisplayMode(0, 0, &tmpMode);
	//		return { tmpMode.w, tmpMode.h };
	//	}

	// private:
	// };

	class IView
	{
	public:
		IView() = default;

	public:
		SDL_FRect bounds = {0.f, 0.f, 0.f, 0.f};
		SDL_FRect min_bounds = {0.f, 0.f, 1.f, 1.f};
		std::string label = "nolabel";
		std::string type = "";
		std::string action = "default";
		std::string id = "null";
		bool required = false;
		bool is_form = false;
		bool prevent_default_behaviour = false;
		bool hidden = false;
		bool disabled = false;
		std::function<void()> onHideCallback = nullptr;
		IView *child=nullptr;
	public:
		IView *getView()
		{
			return this;
		}

		IView* getChildView()
		{
			return child;
		}
		
		IView* setChildView(IView* _child)
		{
			child = _child;
			return child;
		}

		SDL_FRect &getBoundsBox()
		{
			return bounds;
		}

		void setBoundsBox(const SDL_FRect &_bounds, const SDL_FRect &_min_bounds = {0.f})
		{
			bounds = _bounds;
		}

		virtual void updatePosBy(float dx, float dy)
		{
			bounds.x += dx, bounds.y += dy;
		};

		void setOnHide(std::function<void()> _onHideCallback)
		{
			onHideCallback = _onHideCallback;
		}

		IView* toggleView() {
			if (hidden) {
				hidden = false;
				disabled = false;
			}
			else {
				hidden = true;
				disabled = true;
			}
			return this;
		}

		IView* hide()
		{
			if (child) {
				child->hide();
			}
			hidden = true;
			if (onHideCallback)
				onHideCallback();
			return this;
		}

		bool isHidden() { return hidden; }

		IView* show()
		{
			hidden = false;
			return this;
		}

		IView* disable() {
			if (child) {
				child->disable();
			}
			disabled = true; return this;
		}
		IView* enable() {
			if (child) {
				child->enable();
			}
			disabled = false; return this;
		}

		bool isDisabled() { return disabled; }


		virtual bool handleEvent() = 0;

		virtual void Draw() = 0;

		// get %val of context height
		/// @brief
		/// @tparam T
		/// @param val
		/// @return
		template <typename T>
		constexpr inline T ph(const T &val) const { return ((val * bounds.h) / static_cast<decltype(val)>(100)); }

		// get %val of context width
		template <typename T>
		constexpr inline T pw(const T &val) const { return ((val * bounds.w) / static_cast<decltype(val)>(100)); }

		// get %val of ref
		template <typename T>
		constexpr inline T to_cust(const T &val, const T &ref) const { return ((val * ref) / static_cast<decltype(val)>(100)); }

		// get %val of min(a,b)
		template <typename T>
		constexpr inline T to_min(const T &val, const T &a, const T &b)
		{
			if (a < b || a == b)
				return toCust(val, a);
			else if (a > b)
				return toCust(val, b);
		}

	private:
	};

	class Context
	{
	public:
		SDL_Renderer *renderer;
		SDL_Window *window;
		AdaptiveVsync *adaptiveVsync;
		SDL_Event *event;
		// parent view
		IView *pv;
		IView *cv;

		Context() = default;

		Context(Context *_context)
		{
			setContext(_context);
		}

		void setContext(Context *_context)
		{
			renderer = _context->renderer;
			window = _context->window;
			pv = _context->pv;
			adaptiveVsync = _context->adaptiveVsync;
			event = _context->event;
			RedrawTriggeredEvent = _context->RedrawTriggeredEvent;
		}

		void setView(IView *_cv)
		{
			cv = _cv;
		}

		Context *getContext()
		{
			return this;
		}

		void wakeGui()
		{
			SDL_PushEvent(RedrawTriggeredEvent);
		}

	protected:
		SDL_Event *RedrawTriggeredEvent;
	};

	class DisplayInfo : Context
	{
	public:
		DisplayInfo(const DisplayInfo &) = delete;
		DisplayInfo(const DisplayInfo &&) = delete;

		using Context::setContext;

		static DisplayInfo& Get()
		{
			static DisplayInfo instance;
			return instance;
		}

		DeviceDisplayType GetDeviceDisplayType()
		{
			return display_type;
		}

		void initDisplaySystem(const float &display_index = 0.f)
		{
			auto *_mode = SDL_GetCurrentDisplayMode(0);
			int windowWidth, windowHeight;
			int drawableWidth, drawableHeight;
			SDL_GetRendererInfo(renderer, &rendererInfo);
			SDL_Log("MaxTextureW:%d MaxTextureH:%d", rendererInfo.max_texture_width, rendererInfo.max_texture_height);

			// Get window size
			SDL_GetWindowSize(window, &windowWidth, &windowHeight);
			SDL_Log("WINDOW_SIZE H: %d W: %d", windowHeight, windowWidth);
			RenderW = windowWidth;
			RenderH = windowHeight;
			int dispCounts, dc2;
			auto dm = SDL_GetCurrentDisplayMode(1);
			auto ads = SDL_GetDisplays(&dispCounts);

			auto dcs = SDL_GetDisplayContentScale(1);
			// auto dm=SDL_GetDesktopDisplayMode(0);
			auto pd = SDL_GetWindowPixelDensity(window);
			auto ds = SDL_GetWindowDisplayScale(window);
			auto fdm = SDL_GetFullscreenDisplayModes(1, &dc2);
			float dpiScale = (float)dm->w / (float)windowWidth;
			H_DPI = dpiScale;
			dpiScale = (float)dm->h / (float)windowHeight;
			V_DPI = dpiScale;

			SDL_Log("PixelDensity:%f DispScale:%f DispContentScale:%f HDPI:%f VDPI:%f Mode.W:%d Mode.H:%d", pd, ds, dcs, H_DPI, V_DPI, dm->w, dm->h);
			DDPI = 95.f;
#ifdef _WIN32
			HDC screen = GetDC(0);
			float dpiX = static_cast<float>(GetDeviceCaps(screen, LOGPIXELSX));
			float dpiY = static_cast<float>(GetDeviceCaps(screen, LOGPIXELSY));

			float dpi = static_cast<float>((dpiX + dpiY) / 2);
			DDPI = dpi;
			V_DPI = dpiX;
			H_DPI = dpiY;

			std::cout << "General DPI: " << dpi << std::endl;
			std::cout << "DPIX: " << dpiX << std::endl;
			std::cout << "DPIY: " << dpiY << std::endl;

			ReleaseDC(0, screen);
#else
			std::cout << "Not on a Windows environment." << std::endl;
#endif
		}

		void handleEvent()
		{
			if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED or event->type == SDL_EVENT_WINDOW_MAXIMIZED)
			{
				OldRenderW = RenderW;
				OldRenderH = RenderH;
				int newW, newH;
				SDL_GetWindowSize(window, &newW, &newH);
				RenderW = static_cast<float>(newW);
				RenderH = static_cast<float>(newH);
				// SDL_RenderSetLogicalSize(renderer, newW, newH);
				// std::cout << "WR: OW:" << OldRenderW << " OH:" << OldRenderH << " NW:" << RenderW << " NH:" << RenderH << std::endl;
			}
		}

		template <typename T>
		constexpr T toUpdatedWidth(const T &_val)
		{
			return ((T)(RenderW)*_val) / (T)(OldRenderW);
		}

		template <typename T>
		constexpr T toUpdatedHeight(const T &_val)
		{
			return ((T)(RenderH)*_val) / (T)(OldRenderH);
		}

		// get %val of context height
		template <typename T>
		constexpr inline T ph(const T &val) const { return ((val * RenderH) / static_cast<decltype(val)>(100)); }

		// get %val of context width
		template <typename T>
		constexpr inline T pw(const T &val) const { return ((val * RenderW) / static_cast<decltype(val)>(100)); }

		// get %val of ref
		template <typename T>
		constexpr inline T to_cust(const T &val, const T &ref) const { return ((val * ref) / static_cast<decltype(val)>(100)); }

		template <typename T>
		constexpr inline T dpToPx(const T &dp) const
		{
			return (DDPI * dp) / 160.f;
		}

		float RenderH, RenderW, DPRenderH, DPRenderW, DDPI, H_DPI, V_DPI, V_DPI_R, H_DPI_R;
		float OldRenderH, OldRenderW;
		int DrawableH, DrawableW;
		int maxTextureWidth, maxTextureHeight;
		SDL_DisplayMode mode;
		DeviceDisplayType display_type;
		SDL_RendererInfo rendererInfo;

	private:
		DisplayInfo()
		{
			display_type = DeviceDisplayType::UNKOWN;
			DrawableH = 0, DrawableW = 0;
			RenderH = RenderW = DPRenderH = DPRenderW = DDPI = H_DPI = V_DPI = V_DPI_R = H_DPI_R = 1.f;
		}
	};

	class Application : protected Context, IView
	{
	private:
		AdaptiveVsync adaptiveVsync_;
		SDL_Event event_;

	public:
		SDL_Event RedrawTriggeredEvent_;
		UniqueTexture texture;
		std::string PrefLocale, CurrentLocale;

		Application()
		{
			std::setlocale(LC_CTYPE, "en_US.UTF-8");
		}
		using Context::adaptiveVsync;
		using Context::event;
		using Context::getContext;
		using Context::renderer;
		using Context::window;
		using IView::bounds;
		using IView::ph;
		using IView::pw;
		using IView::to_cust;
		bool quit = false;
		bool show_fps = false;

	public:
		short Create(const char *title, int ww, int wh,
					 int window_flags = SDL_WINDOW_HIGH_PIXEL_DENSITY,
					 int renderer_flags = SDL_RENDERER_ACCELERATED /*| SDL_RENDERER_PRESENTVSYNC*/)
		{
			// SDL_Log("Compile Date: %s %s", __DATE__, __TIME__);
			// Init();
			// SDL_GetDisplayMode(0, 0, &DisplayInfo::Get().mode);
			// SDL_Log("DEVICE_SIZE H: %d W: %d", DisplayInfo::Get().mode.h, DisplayInfo::Get().mode.w);
			window = SDL_CreateWindow(title, ww, wh, window_flags);
			// SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN);
			// SDL_SetWindowFullscreen(window, 0);
			// SDL_Delay(1500);
			SDL_Rect usb_b{0};
			/*const int display_index = SDL_GetWindowDisplayIndex(window);
			const int num_modes = SDL_GetNumDisplayModes(display_index);
			SDL_DisplayMode mode;
			if (0 != SDL_GetDisplayMode(display_index, 0, &mode)) {
				SDL_Log("Couldn't get display mode");
			}
			else {
				SDL_SetWindowDisplayMode(window, &mode);
			}*/

			// SDL_SetWindowSize(window, ww,wh);
			// SDL_SetWindowSize(window, mode.w, mode.h);
			// SDL_Log("num modes: %d", num_modes);
			SDL_GetWindowSize(window, &usb_b.w, &usb_b.h);
			SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
			// SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d");
			renderer = SDL_CreateRenderer(window, NULL, renderer_flags);
			// SDL_RenderSetLogicalSize(renderer, ww, wh);
			// SDL_RenderSetIntegerScale(renderer, SDL_TRUE);
			DisplayInfo::Get().setContext(this);
			DisplayInfo::Get().initDisplaySystem(0);
			// SDL_Log("DPI: %f H_DPI: %f V_DPI: %f", DisplayInfo::Get().DDPI, DisplayInfo::Get().H_DPI, DisplayInfo::Get().V_DPI);
			// SDL_Log("WINDOW_SIZE H: %d W: %d", usb_b.h, usb_b.w);
			// DisplayInfo::Get().RenderH = static_cast<float>(usb_b.h), DisplayInfo::Get().RenderW = static_cast<float>(usb_b.w);
			/*DisplayInfo::Get().V_DPI_R = usb_b.h / DisplayInfo::Get().DDPI, DisplayInfo::Get().H_DPI_R = usb_b.w / DisplayInfo::Get().DDPI;
			DisplayInfo::Get().DPRenderH = (DisplayInfo::Get().RenderH * 160.f) / DisplayInfo::Get().DDPI;
			DisplayInfo::Get().DPRenderW = (DisplayInfo::Get().RenderW * 160.f) / DisplayInfo::Get().DDPI;
			//if (SDL_IsTablet())DisplayMetrics::Get().RenderW=(3/4)*DisplayMetrics::Get().RenderW;
			SDL_Log("WindowWidth: %f WindowHeight: %f", DisplayInfo::Get().RenderW, DisplayInfo::Get().RenderH);
			SDL_Log("WindowWidthDP: %f WindowHeightDP: %f", DisplayInfo::Get().DPRenderW, DisplayInfo::Get().DPRenderH);
			SDL_Log("H_DPI_R: %f V_DPI_R: %f", DisplayInfo::Get().H_DPI_R, DisplayInfo::Get().V_DPI_R);
			//SDL_GetRendererOutputSize(renderer, &usb_b.w, &usb_b.h);
			SDL_Log("RENDER_OUTPUT_SIZE H: %d W: %d", usb_b.h, usb_b.w);
			SDL_Log("FrameRate: %d", DisplayInfo::Get().mode.refresh_rate);*/

			bounds.w = DisplayInfo::Get().RenderW;
			bounds.h = DisplayInfo::Get().RenderH;
			pv = this;
			int GL_VER;
			// SDL_GL_GetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, &GL_VER);
			// SDL_Log("MAJOR_GL_VERSION_USED: %d", GL_VER);

			SDL_SetHint(SDL_HINT_RENDER_LINE_METHOD, "1");
			// SDL_SetHint(SDL_HINT_RENDER_LINE_METHOD, "3");
			//  SDL_Log("Frame Rate: %d", SDL_GL_GetSwapInterval());

			// smartFrame_;
			adaptiveVsync = &adaptiveVsync_;
			event = &event_;
			RedrawTriggeredEvent = &RedrawTriggeredEvent_;
			RedrawTriggeredEvent->type = SDL_RegisterEvents(1);
			SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
			DisplayInfo::Get().setContext(this);
			return 1;
		}

		void run()
		{
			tmPrevFrame = SDL_GetTicks();
			loop();
		}

		virtual bool handleEvent() override
		{
			if (adaptiveVsync->pollEvent(event) != 0)
			{
				switch (event->type)
				{
				case SDL_EVENT_QUIT:
					quit = true;
					break;
				default:
					break;
				}
			}
			return false;
		}

		/// @brief
		virtual void Draw() = 0;

		auto getFPS()
		{
			return fps;
		}

		~Application()
		{
			SDL_DestroyRenderer(renderer);
			SDL_DestroyWindow(window);
			renderer = nullptr;
			window = nullptr;
			SDL_Quit();
		}

	private:
		void loop()
		{
			while (not quit)
			{
				frames++;
				handleEvent();
				Draw();
				tmNowFrame = SDL_GetTicks();
				if (tmNowFrame >= tmPrevFrame + 1000)
				{
					fps = frames;
					frames = 0;
					tmPrevFrame = tmNowFrame;
				}
			}
		}

	private:
		uint32_t tmPrevFrame = 0;
		uint32_t tmNowFrame = 0;
		uint32_t frames = 0;
		uint32_t fps = 0;
	};

	struct Margin
	{
		Margin() = default;
		Margin(float _left, float _top, float _right, float _bottom) : left(_left), top(_top), right(_right), bottom(_bottom) {}
		// Left
		float left = 0.f;
		// Top
		float top = 0.f;
		// Right
		float right = 0.f;
		// Bottom
		float bottom = 0.f;
	};

	template <typename T>
	struct Point
	{
		Point(T _x, T _y) : x(_x), y(_y) {}
		T x, y;
	};

	struct Spline
	{
		std::deque<Point<float>> points;

		SDL_FPoint getSplinePoint(float t, bool looped = false)
		{
			int p0, p1, p2, p3;
			if (!looped)
			{
				p1 = static_cast<int>(t) + 1;
				p2 = p1 + 1;
				p3 = p2 + 1;
				p0 = p1 - 1;
			}
			else
			{
				p1 = static_cast<int>(t);
				p2 = (p1 + 1) % points.size();
				p3 = (p2 + 1) % points.size();
				p0 = p1 >= 1 ? p1 - 1 : points.size() - 1;
			}

			t = t - static_cast<int>(t);

			const float tt = t * t;
			const float ttt = tt * t;

			const float q1 = -ttt + 2.0f * tt - t;
			const float q2 = 3.0f * ttt - 5.0f * tt + 2.0f;
			const float q3 = -3.0f * ttt + 4.0f * tt + t;
			const float q4 = ttt - tt;

			const float tx = 0.5f * (points[p0].x * q1 + points[p1].x * q2 + points[p2].x * q3 +
									 points[p3].x * q4);
			const float ty = 0.5f * (points[p0].y * q1 + points[p1].y * q2 + points[p2].y * q3 +
									 points[p3].y * q4);
			return {tx, ty};
		}

		SDL_FPoint getSplineGradient(float t, bool looped = false)
		{
			int p0, p1, p2, p3;
			if (!looped)
			{
				p1 = (int)t + 1;
				p2 = p1 + 1;
				p3 = p2 + 1;
				p0 = p1 - 1;
			}
			else
			{
				p1 = (int)t;
				p2 = (p1 + 1) % points.size();
				p3 = (p2 + 1) % points.size();
				p0 = p1 >= 1 ? p1 - 1 : points.size() - 1;
			}

			t = t - (int)t;

			const float tt = t * t;
			//[[unused]] const float ttt = tt * t;

			const float q1 = -3.0f * tt + 4.0f * t - 1.0f;
			const float q2 = 9.0f * tt - 10.0f * t;
			const float q3 = -9.0f * tt + 8.0f * t + 1.0f;
			const float q4 = 3.0f * tt - 2.0f * t;

			const float tx = 0.5f * (points[p0].x * q1 + points[p1].x * q2 + points[p2].x * q3 +
									 points[p3].x * q4);
			const float ty = 0.5f * (points[p0].y * q1 + points[p1].y * q2 + points[p2].y * q3 +
									 points[p3].y * q4);
			return {tx, ty};
		}
	};

	enum class QUADRANT : uint8_t
	{
		TOP_LEFT,
		TOP_RIGHT,
		BOTTOM_LEFT,
		BOTTOM_RIGHT
	};

	void drawPixelFWeight(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_weight, const SDL_Color &_color)
	{
		const uint8_t alpha_ = static_cast<uint8_t>(_weight * static_cast<float>(_color.a));
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
		SDL_RenderPoint(_renderer, static_cast<int>(_x), static_cast<int>(_y));
	}

	void drawPixelWeight(SDL_Renderer *_renderer, const int &_x, const int &_y, const float &_weight, const SDL_Color &_color)
	{
		const uint8_t alpha_ = static_cast<uint8_t>(_weight * static_cast<float>(_color.a));
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
		SDL_RenderPoint(_renderer, _x, _y);
	}

	void draw_ring(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r, const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		// const std::chrono::steady_clock::time_point tm_start_ = std::chrono::high_resolution_clock::now();
		const float outer_r2_ = _outer_r * _outer_r;
		const float inner_r2_ = _inner_r * _inner_r;
		const float mid1 = inner_r2_ + ((outer_r2_ - inner_r2_) / 2.f);
		const float max = (outer_r2_ - inner_r2_);
		const float max1 = (_outer_r - _inner_r);
		float bias = 1.f;
		float prev_bias = 1.f;
		float res = 1.f;
		std::vector<SDL_FPoint> points_;
		for (float y = 0.5f; y <= _outer_r; y += 1.f)
		{
			for (float x = 0.5f; x <= _outer_r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= outer_r2_ && res >= inner_r2_)
				{
					bias = std::clamp(((1.f - ((fabs(mid1 - res) * 2.f) / max)) * (max1 / 2.f)), 0.f, 1.f);
					points_.emplace_back(SDL_FPoint{(_x - x), (_y - y)});
					points_.emplace_back(SDL_FPoint{(_x + x), (_y - y)});
					points_.emplace_back(SDL_FPoint{(_x - x), (_y + y)});
					points_.emplace_back(SDL_FPoint{(_x + x), (_y + y)});
					if (bias != prev_bias)
					{
						const uint8_t alpha_ = static_cast<uint8_t>(prev_bias * static_cast<float>(_color.a));
						SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
						SDL_RenderPoints(_renderer, points_.data(), points_.size() - 4);
						points_.clear();
						points_.emplace_back(SDL_FPoint{(_x - x), (_y - y)});
						points_.emplace_back(SDL_FPoint{(_x + x), (_y - y)});
						points_.emplace_back(SDL_FPoint{(_x - x), (_y + y)});
						points_.emplace_back(SDL_FPoint{(_x + x), (_y + y)});
					}
					prev_bias = bias;
				}
			}
		}
	}

	void draw_ring_4quad(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_w, const float &_h, const float &_inner_r, const float &_outer_r, const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		const float outer_r2_ = _outer_r * _outer_r;
		const float inner_r2_ = _inner_r * _inner_r;
		const float mid1 = inner_r2_ + ((outer_r2_ - inner_r2_) / 2.f);
		const float max = (outer_r2_ - inner_r2_);
		const float max1 = (_outer_r - _inner_r);
		float bias = 1.f;
		float prev_bias = 1.f;
		float res = 1.f;
		std::vector<SDL_FPoint> points_;
		for (float y = 0.5f; y <= _outer_r; y += 1.f)
		{
			for (float x = 0.5f; x <= _outer_r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= outer_r2_ && res >= inner_r2_)
				{
					bias = std::clamp(((1.f - ((fabs(mid1 - res) * 2.f) / max)) * (max1 / 1.05f)), 0.f, 1.f);
					points_.emplace_back(SDL_FPoint{(_x - x), (_y - y)});
					points_.emplace_back(SDL_FPoint{(_x + x + _w), (_y - y)});
					points_.emplace_back(SDL_FPoint{(_x - x), (_y + y + _h)});
					points_.emplace_back(SDL_FPoint{(_x + x + _w), (_y + y + _h)});
					if (bias != prev_bias)
					{
						const uint8_t alpha_ = static_cast<uint8_t>(prev_bias * static_cast<float>(_color.a));
						SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
						SDL_RenderPoints(_renderer, points_.data(), points_.size() - 4);
						points_.clear();
						points_.emplace_back(SDL_FPoint{(_x - x), (_y - y)});
						points_.emplace_back(SDL_FPoint{(_x + x + _w), (_y - y)});
						points_.emplace_back(SDL_FPoint{(_x - x), (_y + y + _h)});
						points_.emplace_back(SDL_FPoint{(_x + x + _w), (_y + y + _h)});
					}
					prev_bias = bias;
				}
			}
		}
	}

	void draw_ring_top_left_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r,
									 const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		// const std::chrono::steady_clock::time_point tm_start_ = std::chrono::high_resolution_clock::now();
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);
		const float outer_r2_ = _outer_r * _outer_r;
		const float inner_r2_ = _inner_r * _inner_r;
		const float mid1 = inner_r2_ + ((outer_r2_ - inner_r2_) / 2.f);
		const float max = (outer_r2_ - inner_r2_);
		const float max1 = (_outer_r - _inner_r);
		float bias = 1.f;
		float res = 1.f;
		for (float y = 0.5f; y <= _outer_r; y += 1.f)
		{
			for (float x = 0.5f; x <= _outer_r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= outer_r2_ && res >= inner_r2_)
				{
					bias = std::clamp(((1.f - ((fabs(mid1 - res) * 2.f) / max)) * (max1 / 2.f)), 0.f, 1.f);
					drawPixelFWeight(_renderer, _x - x, _y - y, bias, _color);
				}
			}
		}

		// std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - tm_start_);
		// SDL_Log("CDF: %f", dt.count());
	}

	void draw_ring_top_right_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r,
									  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		// const std::chrono::steady_clock::time_point tm_start_ = std::chrono::high_resolution_clock::now();
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);
		const float outer_r2_ = _outer_r * _outer_r;
		const float inner_r2_ = _inner_r * _inner_r;
		const float mid1 = inner_r2_ + ((outer_r2_ - inner_r2_) / 2.f);
		const float max = (outer_r2_ - inner_r2_);
		const float max1 = (_outer_r - _inner_r);
		float bias = 1.f;
		float res = 1.f;
		for (float y = 0.5f; y <= _outer_r; y += 1.f)
		{
			for (float x = 0.5f; x <= _outer_r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= outer_r2_ && res >= inner_r2_)
				{
					bias = std::clamp(((1.f - ((fabs(mid1 - res) * 2.f) / max)) * (max1 / 2.f)), 0.f, 1.f);
					drawPixelFWeight(_renderer, _x + x, _y - y, bias, _color);
				}
			}
		}

		// std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - tm_start_);
		// SDL_Log("CDF: %f", dt.count());
	}

	void draw_ring_bottom_left_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r,
										const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);
		const float outer_r2_ = _outer_r * _outer_r;
		const float inner_r2_ = _inner_r * _inner_r;
		const float mid1 = inner_r2_ + ((outer_r2_ - inner_r2_) / 2.f);
		const float max = (outer_r2_ - inner_r2_);
		const float max1 = (_outer_r - _inner_r);
		float bias = 1.f;
		float res = 1.f;
		for (float y = 0.5f; y <= _outer_r; y += 1.f)
		{
			for (float x = 0.5f; x <= _outer_r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= outer_r2_ && res >= inner_r2_)
				{
					bias = std::clamp(((1.f - ((fabs(mid1 - res) * 2.f) / max)) * (max1 / 2.f)), 0.f, 1.f);
					drawPixelFWeight(_renderer, _x - x, _y + y, bias, _color);
				}
			}
		}
	}

	void draw_ring_bottom_right_quadrand(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r,
										 const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);
		const float outer_r2_ = _outer_r * _outer_r;
		const float inner_r2_ = _inner_r * _inner_r;
		const float mid1 = inner_r2_ + ((outer_r2_ - inner_r2_) / 2.f);
		const float max = (outer_r2_ - inner_r2_);
		const float max1 = (_outer_r - _inner_r);
		float bias = 1.f;
		float res = 1.f;
		for (float y = 0.5f; y <= _outer_r; y += 1.f)
		{
			for (float x = 0.5f; x <= _outer_r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= outer_r2_ && res >= inner_r2_)
				{
					bias = std::clamp(((1.f - ((fabs(mid1 - res) * 2.f) / max)) * (max1 / 2.f)), 0.f, 1.f);
					drawPixelFWeight(_renderer, _x + x, _y + y, bias, _color);
				}
			}
		}
	}

	void draw_ring_quadrand(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r,
							const QUADRANT &_quadrant, const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		const std::chrono::steady_clock::time_point tm_start_ = std::chrono::high_resolution_clock::now();
		if (_quadrant == QUADRANT::TOP_LEFT)
			draw_ring_top_left_quadrant(_renderer, _x, _y, _inner_r, _outer_r, _color);
		else if (_quadrant == QUADRANT::TOP_RIGHT)
			draw_ring_top_right_quadrant(_renderer, _x, _y, _inner_r, _outer_r, _color);
		else if (_quadrant == QUADRANT::BOTTOM_LEFT)
			draw_ring_bottom_left_quadrant(_renderer, _x, _y, _inner_r, _outer_r, _color);
		else if (_quadrant == QUADRANT::BOTTOM_RIGHT)
			draw_ring_bottom_right_quadrand(_renderer, _x, _y, _inner_r, _outer_r, _color);
		//std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - tm_start_);
		//SDL_Log("CDF: %f", dt.count());
	}

	void draw_filled_circle(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
							const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
	{
		// const std::chrono::steady_clock::time_point tm_start_ = std::chrono::high_resolution_clock::now();
		float bias = 1.f;
		float prev_bias = 1.f;
		float res = 1.f;
		const float r2_ = _r * _r;
		const float r_4 = _r / 2.f;
		std::vector<SDL_FPoint> points_;
		for (float y = 0.5f; y <= _r; y += 1.f)
		{
			for (float x = 0.5f; x <= _r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= r2_)
				{
					bias = std::clamp(((1.f - (res / r2_)) * r_4), 0.f, 1.f);

					points_.emplace_back(SDL_FPoint{(_x - x), (_y - y)});
					points_.emplace_back(SDL_FPoint{(_x + x), (_y - y)});
					points_.emplace_back(SDL_FPoint{(_x - x), (_y + y)});
					points_.emplace_back(SDL_FPoint{(_x + x), (_y + y)});
					if (bias != prev_bias)
					{
						const uint8_t alpha_ = static_cast<uint8_t>(prev_bias * static_cast<float>(_color.a));
						SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
						SDL_RenderPoints(_renderer, points_.data(), points_.size() - 4);
						points_.clear();
						points_.emplace_back(SDL_FPoint{(_x - x), (_y - y)});
						points_.emplace_back(SDL_FPoint{(_x + x), (_y - y)});
						points_.emplace_back(SDL_FPoint{(_x - x), (_y + y)});
						points_.emplace_back(SDL_FPoint{(_x + x), (_y + y)});
					}
					prev_bias = bias;
				}
			}
		}
		// std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - tm_start_);
		// SDL_Log("CDF: %f", dt.count());
	}

	void draw_filled_circle_4quad(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_w, const float &_h, const float &_r,
								  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
	{
		// const std::chrono::steady_clock::time_point tm_start_ = std::chrono::high_resolution_clock::now();
		float bias = 1.f;
		float prev_bias = 1.f;
		float res = 1.f;
		const float r2_ = _r * _r;
		const float r_4 = _r / 2.f;
		std::vector<SDL_FPoint> points_;
		for (float y = 0.5f; y <= _r; y += 1.f)
		{
			for (float x = 0.5f; x <= _r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= r2_)
				{
					bias = std::clamp(((1.f - (res / r2_)) * r_4), 0.f, 1.f);
					points_.emplace_back(SDL_FPoint{(_x - x), (_y - y)});
					points_.emplace_back(SDL_FPoint{(_x + x + _w), (_y - y)});
					points_.emplace_back(SDL_FPoint{(_x - x), (_y + y + _h)});
					points_.emplace_back(SDL_FPoint{(_x + x + _w), (_y + y + _h)});
					if (bias != prev_bias)
					{
						const uint8_t alpha_ = static_cast<uint8_t>(prev_bias * static_cast<float>(_color.a));
						SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
						SDL_RenderPoints(_renderer, points_.data(), points_.size() - 4);
						points_.clear();
						points_.emplace_back(SDL_FPoint{(_x - x), (_y - y)});
						points_.emplace_back(SDL_FPoint{(_x + x + _w), (_y - y)});
						points_.emplace_back(SDL_FPoint{(_x - x), (_y + y + _h)});
						points_.emplace_back(SDL_FPoint{(_x + x + _w), (_y + y + _h)});
					}
					prev_bias = bias;
				}
			}
		}
		// std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - tm_start_);
		// SDL_Log("CDF: %f", dt.count());
	}

	void draw_filled_circle_4quad2(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_w, const float &_h, const float &_r,
								   const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
	{
		// const std::chrono::steady_clock::time_point tm_start_ = std::chrono::high_resolution_clock::now();
		float bias = 1.f;
		float prev_bias = 1.f;
		float res = 1.f;
		const float r2_ = _r * _r;
		const float r_4 = _r / 2.f;
		std::vector<SDL_FPoint> points_;
		for (float y = 0.01f; y <= _r; y += 1.f)
		{
			for (float x = 0.01f; x <= _r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= r2_)
				{
					bias = std::clamp(((1.f - (res / r2_)) * r_4), 0.f, 1.f);
					points_.emplace_back(SDL_FPoint{(_x + _r - x), (_y + _r - y)});
					points_.emplace_back(SDL_FPoint{(_x + x + _w - _r), (_y + _r - y)});
					points_.emplace_back(SDL_FPoint{(_x + _r - x), (_y + y + _h - _r)});
					points_.emplace_back(SDL_FPoint{(_x + x + _w - _r), (_y + y + _h - _r)});
					if (bias != prev_bias)
					{
						const uint8_t alpha_ = static_cast<uint8_t>(prev_bias * static_cast<float>(_color.a));
						SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
						SDL_RenderPoints(_renderer, points_.data(), points_.size() - 4);
						points_.clear();
						points_.emplace_back(SDL_FPoint{(_x + _r - x), (_y + _r - y)});
						points_.emplace_back(SDL_FPoint{(_x + x + _w - _r), (_y + _r - y)});
						points_.emplace_back(SDL_FPoint{(_x + _r - x), (_y + y + _h - _r)});
						points_.emplace_back(SDL_FPoint{(_x + x + _w - _r), (_y + y + _h - _r)});
					}
					prev_bias = bias;
				}
			}
		}
		// std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - tm_start_);
		// SDL_Log("CDF: %f", dt.count());
	}

	void draw_circle(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
					 const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
	{
		// const std::chrono::steady_clock::time_point tm_start_ = std::chrono::high_resolution_clock::now();
		draw_ring(_renderer, _x, _y, _r - 1.f, _r + 1.f, _color);
		// std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - tm_start_);
		// SDL_Log("CDF: %f", dt.count());
	}

	void draw_filled_topleft_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
									  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		float bias = 1.f;
		float res = 1.f;
		const float r2_ = _r * _r;
		const float r_4 = _r / 2.f;
		for (float y = 0.5f; y <= _r; y += 1.f)
		{
			for (float x = 0.5f; x <= _r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= r2_)
				{
					bias = std::clamp(((1.f - (res / r2_)) * r_4), 0.f, 1.f);
					drawPixelFWeight(_renderer, _x - x, _y - y, bias, _color);
				}
			}
		}
	}

	void draw_filled_topright_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
									   const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		float bias = 1.f;
		float res = 1.f;
		const float r2_ = _r * _r;
		const float r_4 = _r / 2.f;
		for (float y = 0.5f; y <= _r; y += 1.f)
		{
			for (float x = 0.5f; x <= _r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= r2_)
				{
					bias = std::clamp(((1.f - (res / r2_)) * r_4), 0.f, 1.f);
					drawPixelFWeight(_renderer, _x + x, _y - y, bias, _color);
				}
			}
		}
	}

	void draw_filled_bottomleft_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
										 const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		float bias = 1.f;
		float res = 1.f;
		const float r2_ = _r * _r;
		const float r_4 = _r / 2.f;
		for (float y = 0.5f; y <= _r; y += 1.f)
		{
			for (float x = 0.5f; x <= _r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= r2_)
				{
					bias = std::clamp(((1.f - (res / r2_)) * r_4), 0.f, 1.f);
					drawPixelFWeight(_renderer, _x - x, _y + y, bias, _color);
				}
			}
		}
	}

	void draw_filled_bottomright_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
										  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		float bias = 1.f;
		float res = 1.f;
		const float r2_ = _r * _r;
		const float r_4 = _r / 2.f;
		for (float y = 0.5f; y <= _r; y += 1.f)
		{
			for (float x = 0.5f; x <= _r; x += 1.f)
			{
				res = x * x + y * y;
				if (res <= r2_)
				{
					bias = std::clamp(((1.f - (res / r2_)) * r_4), 0.f, 1.f);
					drawPixelFWeight(_renderer, _x + x, _y + y, bias, _color);
				}
			}
		}
	}

	void draw_filled_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r, const QUADRANT &_quadrant,
							  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		// const std::chrono::steady_clock::time_point tm_start_ = std::chrono::high_resolution_clock::now();
		if (_quadrant == QUADRANT::TOP_LEFT)
			draw_filled_topleft_quadrant(_renderer, _x, _y, _r, _color);
		else if (_quadrant == QUADRANT::TOP_RIGHT)
			draw_filled_topright_quadrant(_renderer, _x, _y, _r, _color);
		else if (_quadrant == QUADRANT::BOTTOM_LEFT)
			draw_filled_bottomleft_quadrant(_renderer, _x, _y, _r, _color);
		else if (_quadrant == QUADRANT::BOTTOM_RIGHT)
			draw_filled_bottomright_quadrant(_renderer, _x, _y, _r, _color);

		// std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - tm_start_);
		// SDL_Log("CDF: %f", dt.count());
	}

	void draw_topleft_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
							   const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		draw_ring_top_left_quadrant(_renderer, _x, _y, _r - 1.f, _r + 1.f, _color);
	}

	void draw_topright_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
								const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		draw_ring_top_right_quadrant(_renderer, _x, _y, _r - 1.f, _r + 1.f, _color);
	}

	void draw_bottomleft_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
								  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		draw_ring_bottom_left_quadrant(_renderer, _x, _y, _r - 1.f, _r + 1.f, _color);
	}

	void draw_bottomright_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
								   const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		draw_ring_bottom_right_quadrand(_renderer, _x, _y, _r - 1.f, _r + 1.f, _color);
	}

	void draw_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r, const QUADRANT &_quadrant,
					   const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		// const std::chrono::steady_clock::time_point tm_start_ = std::chrono::high_resolution_clock::now();
		if (_quadrant == QUADRANT::TOP_LEFT)
			draw_topleft_quadrant(_renderer, _x, _y, _r, _color);
		else if (_quadrant == QUADRANT::TOP_RIGHT)
			draw_topright_quadrant(_renderer, _x, _y, _r, _color);
		else if (_quadrant == QUADRANT::BOTTOM_LEFT)
			draw_bottomleft_quadrant(_renderer, _x, _y, _r, _color);
		else if (_quadrant == QUADRANT::BOTTOM_RIGHT)
			draw_bottomright_quadrant(_renderer, _x, _y, _r, _color);

		// std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - tm_start_);
		// SDL_Log("CDF: %f", dt.count());
	}

	void fillRoundedRectF(SDL_Renderer *_renderer, SDL_FRect _dest, float _rad,
						  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
	{
		// const std::chrono::steady_clock::time_point tm_start_ = std::chrono::high_resolution_clock::now();
		_dest = {roundf(_dest.x), roundf(_dest.y), roundf(_dest.w), roundf(_dest.h)};
		_rad = std::clamp(_rad, 0.f, 99.5f);
		float final_rad = 1.f;
		if (_dest.h > _dest.w)
		{
			final_rad = ((_rad / 2.f) * _dest.w) / 100.f;
		}
		else if (_dest.h <= _dest.w)
		{
			final_rad = ((_rad / 2.f) * _dest.h) / 100.f;
		}

		final_rad = roundf(final_rad);

		if (final_rad >= 100.f && _dest.w == _dest.h)
		{
			draw_filled_circle(_renderer, _dest.x + final_rad, _dest.y + final_rad, final_rad, _color);
			return;
		}

		const std::array<const SDL_FRect, 3> rects_ = {SDL_FRect{(_dest.x), (_dest.y + final_rad), (_dest.w), (_dest.h - (final_rad * 2.f))},
													   {(_dest.x + final_rad), (_dest.y), (_dest.w - (final_rad * 2.f)), (final_rad)},
													   {(_dest.x + final_rad), (_dest.y + _dest.h - final_rad), (_dest.w - (final_rad * 2.f)), (final_rad)}};

		// SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);
		/*SDL_RenderFillRect(_renderer, &mid_rect_);
		SDL_RenderFillRect(_renderer, &top_rect_);
		SDL_RenderFillRect(_renderer, &bottom_rect_);*/
		SDL_RenderFillRects(_renderer, rects_.data(), rects_.size());

		// const SDL_Color& __color = { 0xff, 0, 0, 100 };
		// const SDL_Color& ___color = { 0, 0xff, 0, 0xff };
		// SDL_SetRenderDrawColor(_renderer, ___color.r, ___color.g, ___color.b, ___color.a);
		// SDL_RenderDrawRectF(_renderer, &_dest);
		draw_filled_circle_4quad(_renderer, _dest.x + final_rad-1.f, _dest.y + final_rad, _dest.w - (final_rad * 2.f), _dest.h - (final_rad * 2.f), final_rad, _color);
		// draw_filled_circle_4quad2(_renderer, _dest.x, _dest.y, _dest.w, _dest.h, final_rad, _color);
		// std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - tm_start_);
		// SDL_Log("CDF: %f", dt.count());
	}

	void drawRoundedRectF(SDL_Renderer *_renderer, const SDL_FRect &_dest, const float &_rad,
						  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
	{
		float final_rad = 1.f;
		if (_dest.h > _dest.w)
		{
			final_rad = ((_rad / 2.f) * _dest.w) / 100.f;
		}
		else if (_dest.h <= _dest.w)
		{
			final_rad = ((_rad / 2.f) * _dest.h) / 100.f;
		}

		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);
		SDL_RenderLine(_renderer, _dest.x + final_rad, _dest.y, _dest.x + _dest.w - final_rad, _dest.y);
		SDL_RenderLine(_renderer, _dest.x + final_rad, _dest.y + _dest.h, _dest.x + _dest.w - final_rad, _dest.y + _dest.h);
		SDL_RenderLine(_renderer, _dest.x + _dest.w, _dest.y + final_rad, _dest.x + _dest.w, _dest.y + _dest.h - final_rad);
		SDL_RenderLine(_renderer, _dest.x, _dest.y + final_rad, _dest.x, _dest.y + _dest.h - final_rad);

		draw_quadrant(_renderer, _dest.x + final_rad + 1.f, _dest.y + final_rad + 1.f, final_rad, QUADRANT::TOP_LEFT, _color);
		draw_quadrant(_renderer, _dest.x + _dest.w - final_rad, _dest.y + final_rad + 1.f, final_rad, QUADRANT::TOP_RIGHT, _color);
		draw_quadrant(_renderer, _dest.x + final_rad + 1.f, _dest.y + _dest.h - final_rad, final_rad, QUADRANT::BOTTOM_LEFT, _color);
		draw_quadrant(_renderer, _dest.x + _dest.w - final_rad, _dest.y + _dest.h - final_rad, final_rad, QUADRANT::BOTTOM_RIGHT, _color);
	}

	void fillRoundedRectOutline(SDL_Renderer *_renderer, const SDL_FRect &rect, const float &_r, const float &_outline_psz, const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
	{
		float final_rad = 1.f;
		float outline_sz_ = 1.f;
		SDL_FRect _rect = {roundf(rect.x), roundf(rect.y), roundf(rect.w), roundf(rect.h)};
		if (_rect.h > _rect.w)
		{
			final_rad = ((_r / 2.f) * _rect.w) / 100.f;
			outline_sz_ = ((_outline_psz / 2.f) * _rect.w) / 100.f;
		}
		else if (_rect.h <= _rect.w)
		{
			final_rad = ((_r / 2.f) * _rect.h) / 100.f;
			outline_sz_ = ((_outline_psz / 2.f) * _rect.h) / 100.f;
		}
		final_rad = roundf(final_rad);
		outline_sz_ = roundf(outline_sz_);

		const std::array<SDL_FRect, 4> side_rects = {SDL_FRect{_rect.x + final_rad, _rect.y, _rect.w - (final_rad * 2.f), outline_sz_},
													 {_rect.x + final_rad, _rect.y + _rect.h - outline_sz_, _rect.w - (final_rad * 2.f), outline_sz_},
													 {_rect.x, _rect.y + final_rad, outline_sz_, _rect.h - (final_rad * 2.f)},
													 {_rect.x + _rect.w - outline_sz_, _rect.y + final_rad, outline_sz_, _rect.h - (final_rad * 2.f)}};
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);
		SDL_RenderFillRects(_renderer, side_rects.data(), side_rects.size());
		draw_ring_4quad(_renderer, _rect.x + final_rad-1, _rect.y + final_rad, side_rects[0].w, side_rects[3].h, final_rad - outline_sz_, final_rad, _color);
	}

	void DrawCircle(SDL_Renderer *renderer, float x, float y, float r,
					SDL_Color color = {0xff, 0xff, 0xff, 0xff})
	{
		// auto start = std::chrono::high_resolution_clock::now();
		draw_circle(renderer, x, y, r, color);
		// SDL_SetRenderDrawColor(renderer, cache_color_r, cache_color_g, cache_color_b, cache_color_a);
		// std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - start);
		// SDL_Log("T: %f", dt.count());
	}

	void FillCircle(SDL_Renderer *renderer, float x, float y, float r,
					SDL_Color color = {0xff, 0xff, 0xff, 0xff}, Uint8 quadrant = 0)
	{
		// auto start = std::chrono::high_resolution_clock::now();
		draw_filled_circle(renderer, x, y, r, color);
		// filledEllipse(renderer, x, y, r,r, color, quadrant);
		// SDL_SetRenderDrawColor(renderer, cache_color_r, cache_color_g, cache_color_b, cache_color_a);
		// std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - start);
		// SDL_Log("T: %f", dt.count());
	}

	void renderClear(SDL_Renderer *renderer, const uint8_t &red, const uint8_t &green,
					 const uint8_t &blue,
					 const uint8_t &alpha = 0xFF)
	{
		// SDL_GetRenderDrawColor(renderer, &CACHE_COL.r, &CACHE_COL.g, &CACHE_COL.g, &CACHE_COL.a);
		SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
		SDL_RenderClear(renderer);
		// SDL_SetRenderDrawColor(renderer, CACHE_COL.r, CACHE_COL.g, CACHE_COL.g, CACHE_COL.a);
	}

	namespace Experimental
	{
		namespace ImageManip
		{
			uint32_t getCommonColor(SDL_Surface *_surface)
			{
				SDL_Surface *cv_surf;
				cv_surf = SDL_ConvertSurfaceFormat(_surface, SDL_PIXELFORMAT_RGBA8888);
				uint32_t res = 0;
				int w = cv_surf->w;
				int h = cv_surf->h;
				std::span pixels{(uint32_t *)(cv_surf->pixels), (uint32_t)(w * h)};
				std::cout << w << " " << h << std::endl;

				union ColorPallet
				{
					uint32_t cl;
					uint8_t i[4];
				};

				ColorPallet clr;
				uint64_t r = 0, g = 0, b = 0, a = 0;

				for (const auto &px : pixels)
				{
					clr.cl = px;
					r += clr.i[1];
					g += clr.i[2];
					b += clr.i[3];
					a += clr.i[0];
				}

				clr.i[1] = b / (w * h);
				clr.i[2] = g / (w * h);
				clr.i[3] = r / (w * h);
				clr.i[0] = a / (w * h);
				std::cout << "R:" << r / (w * h) << " G:" << g / (w * h) << " B:" << b / (w * h) << " A:" << a / (w * h) << std::endl;
				res = clr.cl;
				SDL_DestroySurface(cv_surf);
				return res;
			}
		};
		namespace Gradient
		{
			constexpr auto lerp_colors(const float colors[2][3], const float &value)
			{
				return std::array{
					colors[0][0] + (colors[1][0] - colors[0][0]) * value,
					// std::lerp(colors[0][0],(colors[1][0], value),
					colors[0][1] + (colors[1][1] - colors[0][1]) * value,
					colors[0][2] + (colors[1][2] - colors[0][2]) * value};
			}

			void fillGradientRect(SDL_Renderer *renderer, const SDL_FRect &_rect, const SDL_Color &_left, const SDL_Color &_right)
			{
				const float rt = 1.f / 255.f;
				float norm_colors[2][3] = {{rt * _left.r, rt * _left.g, rt * _left.b}, {rt * _right.r, rt * _right.g, rt * _right.b}};
				float t = 0.f;
				const float dt = 1.f / _rect.w;
				// SDL_Color result;
				for (float x = 0.f; x < _rect.w; x += 1.f)
				{
					const auto tmp = lerp_colors(norm_colors, t);
					const SDL_Color result = {(uint8_t)(tmp[0] * 255.f), (uint8_t)(tmp[1] * 255.f), (uint8_t)(tmp[2] * 255.f), 0xff};
					SDL_SetRenderDrawColor(renderer, result.r, result.g, result.b, result.a);
					SDL_RenderLine(renderer, _rect.x + x, _rect.y, _rect.x + x, _rect.y + _rect.h);
					t += dt;
				}
			}

			auto rotate(const std::array<float, 2> &position, const float &angle) noexcept
			{
				return std::array{
					cosf(angle) * position[0] - sinf(angle) * position[1],
					sinf(angle) * position[0] + cosf(angle) * position[1]};
			}
			constexpr auto translate(const std::array<float, 2> &position, const float &x, const float &y) noexcept
			{
				return std::array{position[0] + x, position[1] + y};
			}

			void fillGradientRectAngle(SDL_Renderer *renderer, const SDL_FRect &_rect, const float &_angle, const SDL_Color &_left, const SDL_Color &_right)
			{
				const float rt = 1.f / 255.f;
				float norm_colors[2][3] = {{rt * _left.r, rt * _left.g, rt * _left.b}, {rt * _right.r, rt * _right.g, rt * _right.b}};
				float t = 0.f;
				const float dt = 1.f / _rect.w;
				const auto mx = _rect.w / 2.f;
				const auto my = _rect.h / 2.f;

				// SDL_Color result;
				for (float x = 0.f; x < _rect.w; x += 1.f)
				{
					for (float y = 0.f; y < _rect.h; y += 1.f)
					{
						const std::array<float, 2> tp = translate(rotate(translate({x, y}, -mx, -my), _angle), mx, my);
						const auto tmp = lerp_colors(norm_colors, (1.f / _rect.w) * std::clamp(tp[0], 0.f, _rect.w));
						const SDL_Color result = {(uint8_t)(tmp[0] * 255.f), (uint8_t)(tmp[1] * 255.f), (uint8_t)(tmp[2] * 255.f), 0xff};
						SDL_SetRenderDrawColor(renderer, result.r, result.g, result.b, result.a);
						SDL_RenderPoint(renderer, _rect.x + x, _rect.y + y);
					}
				}
				// SDL_Log("%d,%d,%d : %d,%d,%d", _left.r, _left.g, _left.b, _right.r, _right.g, _right.b);
				// SDL_Log("%f", _angle);
			}

			void fillGradientTexture(SDL_Renderer *renderer, SDL_Texture *_texture, const float &_angle, const SDL_Color &_left, const SDL_Color &_right)
			{
				int w, h, pitch;
				SDL_QueryTexture(_texture, 0, 0, &w, &h);
				const float rt = 1.f / 255.f;
				const float norm_colors[2][3] = {{rt * _left.r, rt * _left.g, rt * _left.b}, {rt * _right.r, rt * _right.g, rt * _right.b}};
				float t = 0.f;
				const float dt = 1.f / w;
				const auto mx = w / 2.f;
				const auto my = h / 2.f;

				union PUN
				{
					uint32_t _rd;
					uint8_t bs[4];
				} pal;

				void *pixels;
				SDL_LockTexture(_texture, nullptr, &pixels, &pitch);
				uint32_t *rdata = (uint32_t *)(pixels);
				for (int x = 0; x < w; x++)
				{
					for (int y = 0; y < h; y++)
					{
						const auto tp = translate(rotate(translate({(float)x, (float)y}, -mx, -my), _angle), mx, my);
						const auto tmp = lerp_colors(norm_colors, (1.f / (float)w) * std::clamp(tp[0], 0.f, (float)w));
						const SDL_Color result = {(uint8_t)(tmp[0] * 255.f), (uint8_t)(tmp[1] * 255.f), (uint8_t)(tmp[2] * 255.f), 0xff};
						pal.bs[0] = result.a;
						pal.bs[1] = result.r;
						pal.bs[2] = result.g;
						pal.bs[3] = result.b;
						rdata[(y * w) + x] = pal._rd;
					}
				}

				SDL_UnlockTexture(_texture);
				// SDL_Log("%d,%d,%d : %d,%d,%d", _left.r, _left.g, _left.b, _right.r, _right.g, _right.b);
				// SDL_Log("%f", _angle);
			}
		};

		union UINT32PALLETE_UNON
		{
			uint32_t ui32data;
			uint8_t ui8data[4];
		};

		auto blurFunc = [](Uint32 *data, const int &dw, const int &dh, const int &blur_extent)
		{
			UINT32PALLETE_UNON CD;
			Uint32 color;
			Uint32 rb = 0, gb = 0, bb = 0, ab = 0;
			int numPix = 0;

			for (int y = 0; y < dh; ++y)
			{
				for (int x = 0; x < dw; ++x)
				{
					CD.ui32data = data[(y * dw) + x];
					color = CD.ui32data;
					// SDL_GetRGBA() is a method for getting color
					// components from a 32 bit color
					// Uint8 r = CD.c[1], g = CD.c[2], b = CD.c[3], a = CD.c[0];
					// SDL_GetRGBA(color, SDL_PIXELFORMAT_RGBA8888, &r, &g, &b, &a);

					rb = 0, gb = 0, bb = 0, ab = 0;

					// Within the two for-loops below, colors of adjacent pixels are added up

					for (int yo = -blur_extent; yo <= blur_extent; yo++)
					{
						for (int xo = -blur_extent; xo <= blur_extent; xo++)
						{
							if (y + yo >= 0 && x + xo >= 0 && y + yo < dh && x + xo < dw)
							{
								CD.ui32data = data[(((y + yo) * dw) + (x + xo))];

								rb += CD.ui8data[1];
								gb += CD.ui8data[2];
								bb += CD.ui8data[3];
								ab += CD.ui8data[0];
							}
						}
					}

					// The sum is then, divided by the total number of
					// pixels present in a block of blur radius

					// For blur_extent 1, it will be 9
					// For blur_extent 2, it will be 25
					// and so on...

					// In this way, we are getting the average of
					// all the pixels in a block of blur radius

					//(((blur_extent * 2) + 1) * ((blur_extent * 2) + 1)) calculates
					// the total number of pixels present in a block of blur radius

					// Bit shifting color bits to form a 32 bit proper colour
					// color = (r) | (g << 8) | (b << 16) | (a << 24);
					numPix = (((blur_extent * 2) + 1) * ((blur_extent * 2) + 1));
					CD.ui8data[0] = (Uint8)(ab / numPix);
					CD.ui8data[1] = (Uint8)(rb / numPix);
					CD.ui8data[2] = (Uint8)(gb / numPix);
					CD.ui8data[3] = (Uint8)(bb / numPix);
					color = CD.ui32data;
					data[(y * dw) + x] = color;
				}
			}
		};

		void blurIMG(SDL_Surface *imageSurface, const int &blurExtend, const int &iterations = 1) // This manipulates with SDL_Surface and gives it box blur effect
		{
			const auto start = std::chrono::high_resolution_clock::now();
			Uint32 *data = ((Uint32 *)imageSurface->pixels);
			for (int i = 0; i < iterations; ++i)
				blurFunc(data, imageSurface->w, imageSurface->h, blurExtend);
			std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - start);
			SDL_Log("bluring done: %f secs", dt.count());
		}
	};

	struct Rect : Context
	{
		SDL_FRect rect;
		SDL_Color color;
		float corner_rad = 0.f;

		Rect() = default;

		Rect(Context *_context, const SDL_FRect &_rect, const SDL_Color &_color, const float &_corner_rad) /*:rect(_rect),color(_color)*/
		{
			Build(_context, _rect, _color, _corner_rad);
		}

		Rect &Build(Context *_context, const SDL_FRect &_rect, const SDL_Color &_color, const float &_corner_rad)
		{
			setContext(_context);
			rect = _rect;
			color = _color;
			corner_rad = _corner_rad;
			return *this;
		}

		void Draw()
		{
			Uint8 r_, g_, b_, a_;
			SDL_GetRenderDrawColor(renderer, &r_, &g_, &b_, &a_);
			SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
			fillRoundedRectF(renderer, rect, corner_rad, color);
			SDL_SetRenderDrawColor(renderer, r_, g_, b_, a_);
		}
	};

	class RectOutline : public Context, IView
	{
	private:
		SDL_FRect inner_rect{};

	public:
		using IView::getView;
		using IView::isHidden;
		using IView::show;
		using IView::hide;
		SDL_FRect rect{};
		SDL_Color color{}, outline_color{};
		float corner_rad = 0.f, outline = 0.f;

		RectOutline() = default;

		RectOutline(Context *_context, const SDL_FRect &_rect, const float &_outline, const float &_corner_rad, const SDL_Color &_color, const SDL_Color &_outline_color)
		{
			Build(_context, _rect, _outline, _corner_rad, _color, _outline_color);
		}

		RectOutline &Build(Context *_context, const SDL_FRect &_rect, const float &_outline, const float &_corner_rad, const SDL_Color &_color, const SDL_Color &_outline_color)
		{
			setContext(_context);
			rect = _rect;
			color = _color;
			outline_color = _outline_color;
			outline = _outline;
			corner_rad = _corner_rad;

			float outln_sz = 0.f;
			if (_rect.h > _rect.w)
			{
				outln_sz = ((_outline / 2.f) * _rect.w) / 100.f;
			}
			else if (_rect.h <= _rect.w)
			{
				outln_sz = ((_outline / 2.f) * _rect.h) / 100.f;
			}

			inner_rect =
				{
					rect.x + outln_sz,
					rect.y + outln_sz,
					rect.w - (outln_sz * 2.f),
					rect.h - (outln_sz * 2.f)};

			return *this;
		}

		bool handleEvent() override
		{
			if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
			{
				rect =
					{
						DisplayInfo::Get().toUpdatedWidth(rect.x),
						DisplayInfo::Get().toUpdatedHeight(rect.y),
						DisplayInfo::Get().toUpdatedWidth(rect.w),
						DisplayInfo::Get().toUpdatedHeight(rect.h)};
				inner_rect =
					{
						DisplayInfo::Get().toUpdatedWidth(inner_rect.x),
						DisplayInfo::Get().toUpdatedHeight(inner_rect.y),
						DisplayInfo::Get().toUpdatedWidth(inner_rect.w),
						DisplayInfo::Get().toUpdatedHeight(inner_rect.h)};
			}
			return false;
		}

		void Draw() override
		{
			// Uint8 r_, g_, b_, a_;
			// SDL_GetRenderDrawColor(renderer, &r_, &g_, &b_, &a_);
			fillRoundedRectF(renderer, rect, corner_rad, color);
			fillRoundedRectOutline(renderer, rect, corner_rad, outline, outline_color);
			// SDL_SetRenderDrawColor(renderer, r_, g_, b_, a_);
		}
	};

	/**Give prog fake work to do while waiting for focus regain.
	 * Called by Volt::checkAppFocus() when your app enters the background.
	 * Helps to recover all your textures when your app gains focus again.
	 * */
	void safePauseApp(const int &ms)
	{
		SDL_Log("Volt::Application has lost focus!");
		SDL_Event paEvent;
		bool paDone = 0;
		while (!paDone)
		{
			while (SDL_PollEvent(&paEvent) != 0)
			{
				switch (paEvent.type)
				{
				case SDL_EVENT_DID_ENTER_FOREGROUND:
					SDL_Log("Volt::app has gained focus!");
					paDone = 1;
					break;
				default:
					break;
				}
			}
			if (ms > 0)
				SDL_Delay(ms);
		}
	}

	/// Call this if you want your app loop to keep running even when in background and dont want to lose your textures on APP_DID_ENTER_FOREGROUND
	///@param ms ->refresh rate delay in micro seconds
	int checkAppFocus(SDL_Event &caf, const int &ms = 15)
	{
		if (caf.type == SDL_EVENT_WILL_ENTER_BACKGROUND)
		{
			Volt::safePauseApp(ms);
			return 1;
		}
		return 0;
	}

	class Interpolator
	{
	public:
		Interpolator &start(float initial_velocity)
		{
			vO = initial_velocity;
			MAX_LEN = 100.f * ((vO * vO) / (2 * std::fabs(ADG)));
			value = 0.f, prev_dy = 0.f;
			update_physics = true;
			tm_start = SDL_GetTicks();
			return *this;
		}

		Interpolator &startWithDistance(float max_length)
		{
			MAX_LEN = max_length / 100.f;
			vO = std::sqrt((2 * std::fabs(ADG)) * MAX_LEN);
			vO *= 2.f;
			MAX_LEN = max_length;
			value = 0.f, prev_dy = 0.f;
			update_physics = true;
			tm_start = SDL_GetTicks();
			return *this;
		}

		Interpolator &update()
		{
			if (!update_physics)
				return *this;
			float t = (SDL_GetTicks() - tm_start) / 1000.f;
			// t *= 1.5f;

			dy = ((vO * t) + ((0.5f * ADG) * (t * t))) * 100.f;
			value = dy - prev_dy;
			prev_dy = dy;
			// SDL_Log("%f", value);
			if (dy >= MAX_LEN - 1.f)
				update_physics = false;
			return *this;
		}

		Interpolator &setIsAnimating(bool animating) noexcept
		{
			update_physics = animating;
			if (animating == false)
				value = 0;
			return *this;
		}

		float getValue() const noexcept
		{
			return value;
		}

		bool isAnimating() const noexcept
		{
			return update_physics;
		}

	protected:
		uint32_t tm_start = 0;
		bool update_physics = false;
		float ADG = -9.8f;
		float MAX_LEN = 0.f;
		float vO = 0.f;
		float dy = 0.f;
		float prev_dy = 0.f;
		float value = 0.f;
		float dy_sum = 0.f;
	};

	class DecelerateInterpolator
	{
	};

	class AccelerateInterpolator
	{
	};

	class AccelerateDecelerateInterpolator
	{
	};

	enum class AnimationType
	{
		SlideXY,
		SlideX,
		SlideY,
		Fade,
		Then
	};

	class Animator : protected Interpolator
	{
	public:
		Animator &then(/*delay: ms*/)
		{
			return *this;
		}

	private:
		std::deque<AnimationType> anim_pattern;
		bool is_animating = false;
	};

	namespace Experimental
	{

		class Wave : public Context
		{
		public:
			std::deque<SDL_FRect> points;
			Wave &setContext(Context *context_)
			{
				Context::setContext(context_);
				return *this;
			}

			Wave &Build(const SDL_FRect &rect, const SDL_Color &wave_color)
			{
				dest = rect;
				waveColor = wave_color;
				r = dest.h / 2.f;
				for (int i = 0; i < dest.w; ++i)
				{
					points.push_back({dest.x + i, (dest.y + r + r * sinf(t))});
					t += ti;
				}
				tmLsFrame = SDL_GetTicks();
				return *this;
			}

			auto updateWidth(float newWidth) -> Wave &
			{
				if (newWidth == dest.w)
					return *this;
				if (newWidth < 1)
					newWidth = 1;
				if (newWidth > dest.w)
				{
					dest.w = newWidth;
					for (int d = points.back().x + 1; d < dest.x + dest.w; ++d)
					{
						points.push_back({static_cast<float>(d), (dest.y + r + r * sinf(t))});
						t += ti;
					}
				}
				if (newWidth < dest.w)
				{
					decrementWidthBy(dest.w - newWidth);
				}
				return *this;
			}

			Wave &incrementWidthBy(float incVal)
			{
				dest.w += incVal;
				for (int d = points.back().x + 1; d < dest.x + dest.w; ++d)
				{
					points.push_back({(float)d, (dest.y + r + r * sinf(t))});
					t += ti;
				}
				return *this;
			}

			Wave &decrementWidthBy(float incVal)
			{
				if (dest.w - incVal < 1)
					return *this;
				dest.w -= incVal;
				while (points.back().x >= dest.x + dest.w)
				{
					points.pop_back();
					t -= ti;
				}
				return *this;
			}

			Wave &setAnimationDelay(const MilliSec<int> &anim_delay) noexcept
			{
				this->animDelay = anim_delay.get();
				return *this;
			}

			Wave &setWavePointSize(const float &point_size) noexcept
			{
				this->wavePoint.w = this->wavePoint.h = point_size;
				return *this;
			}

			Wave &Draw()
			{
				if (SDL_GetTicks() - tmLsFrame > animDelay)
				{
					points.emplace_back(SDL_FRect{points.back().x + 1.f, (dest.y + r + (r * sinf(t)))});
					std::for_each(points.begin(), points.end(), [](SDL_FRect &p)
								  { p.x--; });
					if (points.front().x < dest.x)
						points.pop_front();
					t += ti;
					tmLsFrame = SDL_GetTicks();
				}
				SDL_SetRenderDrawColor(renderer, waveColor.r, waveColor.g, waveColor.b, waveColor.a);
				for (auto &point : points)
				{
					wavePoint.x = point.x, wavePoint.y = point.y;
					SDL_RenderFillRect(renderer, &wavePoint);
					// SDL_RenderPoint(renderer, points[c].x, points[c].y);
				}
				return *this;
			}

		private:
			float r = 0.f;
			double t = 0.0, ti = 0.15;
			Uint32 tmLsFrame, animDelay = 1;
			SDL_FRect dest, wavePoint = {0.f, 0.f, 2.f, 2.f};
			SDL_Color waveColor = {0xff, 0xff, 0xff, 0xff};
		};
	}

	class TextAttributes
	{
	public:
		TextAttributes() = default;

		TextAttributes(const std::string &a_text, const SDL_Color a_text_col,
					   const SDL_Color &a_background_col) : text(a_text), text_color(a_text_col),
															bg_color(a_background_col) {}

		std::string text;
		SDL_Color bg_color;
		SDL_Color text_color;

		void setTextAttributes(const TextAttributes &text_attributes)
		{
			this->text = text_attributes.text;
			this->bg_color = text_attributes.bg_color;
			this->text_color = text_attributes.text_color;
		}

		TextAttributes &setText(const char *_text)
		{
			this->text = _text;
			return *this;
		}

		TextAttributes &setTextColor(const SDL_Color &_textcolor)
		{
			this->text_color = _textcolor;
			return *this;
		}

		TextAttributes &setTextBgColor(const SDL_Color &_text_bg_color)
		{
			this->bg_color = _text_bg_color;
			return *this;
		}
	};

	class U8TextAttributes
	{
	public:
		U8TextAttributes() = default;

		U8TextAttributes(const std::u8string &a_text, const SDL_Color a_text_col,
						 const SDL_Color &a_background_col) : text(a_text), text_color(a_text_col),
															  bg_color(a_background_col) {}

		std::u8string text;
		SDL_Color bg_color;
		SDL_Color text_color;

		void setTextAttributes(const U8TextAttributes &text_attributes)
		{
			this->text = text_attributes.text;
			this->bg_color = text_attributes.bg_color;
			this->text_color = text_attributes.text_color;
		}

		U8TextAttributes &setText(const char8_t *_text)
		{
			this->text = _text;
			return *this;
		}

		U8TextAttributes &setTextColor(const SDL_Color &_textcolor)
		{
			this->text_color = _textcolor;
			return *this;
		}

		U8TextAttributes &setTextBgColor(const SDL_Color &_text_bg_color)
		{
			this->bg_color = _text_bg_color;
			return *this;
		}
	};

	class FontAttributes
	{
	public:
		FontAttributes() = default;

		FontAttributes(const std::string &a_font_file, const FontStyle &a_font_style,
					   const uint8_t &a_font_size) : font_file(a_font_file), font_size(a_font_size), font_style(a_font_style) {}

		std::string font_file;
		uint8_t font_size = 0xff;
		FontStyle font_style;

		void setFontAttributes(const FontAttributes &font_attributes)
		{
			this->font_file = font_attributes.font_file,
			this->font_size = font_attributes.font_size,
			this->font_style = font_attributes.font_style;
		}

		FontAttributes &setFontStyle(const FontStyle &a_fontstyle)
		{
			this->font_style = a_fontstyle;
			return *this;
		}

		FontAttributes &setFontFile(const std::string &a_fontfile)
		{
			this->font_file = a_fontfile;
			return *this;
		}

		FontAttributes &setFontSize(const uint8_t &a_fontsize)
		{
			this->font_size = a_fontsize;
			return *this;
		}
	};

	enum class Font
	{
		ConsolasBold,
		OpenSansRegular,
		OpenSansSemiBold,
		OpenSansBold,
		OpenSansExtraBold,
		RobotoBold,
		SegoeUiEmoji,
		// YuGothBold,
	};

	struct MemFont
	{
		const unsigned char *font_data;
		unsigned int font_data_size;
		int font_size;
		std::string font_name;
	};

	MemFont ConsolasBold{
		.font_data = mem_ft_consolas_bold_data,
		.font_data_size = ff_consolas_bold_len,
		.font_size = 255,
		.font_name = "consolas-bold"};

	MemFont OpenSansRegular{
		.font_data = ff_OpenSans_Regular_ttf_data,
		.font_data_size = ff_OpenSans_Regular_ttf_len,
		.font_size = 255,
		.font_name = "opensans-regular"};

	MemFont OpenSansSemiBold{
		.font_data = ff_OpenSans_SemiBold_ttf_data,
		.font_data_size = ff_OpenSans_SemiBold_ttf_len,
		.font_size = 255,
		.font_name = "opensans-semi-bold"};

	MemFont OpenSansBold{
		.font_data = ff_OpenSans_Bold_ttf_data,
		.font_data_size = ff_OpenSans_Bold_ttf_len,
		.font_size = 255,
		.font_name = "opensans-bold"};

	MemFont OpenSansExtraBold{
		.font_data = ff_OpenSans_ExtraBold_ttf_data,
		.font_data_size = ff_OpenSans_ExtraBold_ttf_len,
		.font_size = 255,
		.font_name = "opensans-extra-bold"};

	MemFont RobotoBold{
		.font_data = ff_Roboto_Bold_ttf_data,
		.font_data_size = ff_Roboto_Bold_ttf_len,
		.font_size = 255,
		.font_name = "roboto-bold"};

	MemFont SegoeUiEmoji{
		.font_data = ff_seguiemj_ttf_data,
		.font_data_size = ff_seguiemj_ttf_len,
		.font_size = 255,
		.font_name = "segoe-ui-emoji"};

	/*MemFont YuGothBold{
		.font_data = ff_YuGothB_ttc_data,
		.font_data_size = ff_YuGothB_ttc_len,
		.font_size = 255,
		.font_name = "yugoth-bold"
	};*/

	[[maybeunused]] std::unordered_map<Font, MemFont *> Fonts{
		{Font::ConsolasBold, &ConsolasBold},
		{Font::OpenSansRegular, &OpenSansRegular},
		{Font::OpenSansSemiBold, &OpenSansSemiBold},
		{Font::OpenSansBold, &OpenSansBold},
		{Font::OpenSansExtraBold, &OpenSansExtraBold},
		{Font::RobotoBold, &RobotoBold},
		{Font::SegoeUiEmoji, &SegoeUiEmoji},
		//{Font::YuGothBold,&YuGothBold},
	};

	class FontStore
	{
	public:
		~FontStore()
		{
			for (auto &[path_, font] : fonts)
				TTF_CloseFont(font);
		}

		TTF_Font *operator[](const std::pair<std::string, int> &font)
		{
			const std::string key = font.first + std::to_string(font.second);
			if (fonts.count(key) == 0)
			{
				if (!(fonts[key] = TTF_OpenFont(font.first.c_str(), font.second)))
				{
					// SDL_Log("%s",/* key.c_str(),*/ TTF_GetError());
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", /* key.c_str(),*/ TTF_GetError());
					fonts.erase(key);
					// TTF_OpenFontDPI()
					return nullptr;
				}
				else
				{
					// SDL_Log("NEW FONT: %s", key.c_str());
					// TTF_SetFontSDF(fonts[key], SDL_TRUE);
					// TTF_SetFontOutline(fonts[key], m_font_outline);
					// TTF_SetFontKerning(fonts[key], 1);
					// TTF_SetFontHinting(fonts[key], TTF_HINTING_LIGHT_SUBPIXEL);
					// TTF_SetFontHinting(fonts[key], TTF_HINTING_MONO);
				}
			}
			return fonts[key];
		}

		TTF_Font *operator[](const MemFont &mem_font)
		{
			const std::string key = mem_font.font_name + std::to_string(mem_font.font_size);
			if (fonts.count(key) == 0)
			{
				SDL_RWops *rw_ = SDL_RWFromConstMem(mem_font.font_data, mem_font.font_data_size);
				// if (!rw_)SDL_Log("failed to rwops");
				if (!(fonts[key] = TTF_OpenFontRW(rw_, SDL_TRUE, mem_font.font_size)))
				{
					// SDL_Log("%s",/* key.c_str(),*/ TTF_GetError());
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", /* key.c_str(),*/ TTF_GetError());
					fonts.erase(key);
					// TTF_OpenFontDPI()
					return nullptr;
				}
				else
				{
					SDL_Log("NEW FONT: %s", key.c_str());
					// TTF_SetFontSDF(fonts[key], SDL_TRUE);
					// TTF_SetFontOutline(fonts[key], m_font_outline);
					// TTF_SetFontKerning(fonts[key], 1);
					// TTF_SetFontHinting(fonts[key], TTF_HINTING_MONO);
				}
			}
			return fonts[key];
		}

	private:
		std::unordered_map<std::string, TTF_Font *> fonts;
	};

	class U8FontStore
	{
	public:
		~U8FontStore()
		{
			for (auto &[font_path, font] : fonts)
				TTF_CloseFont(font);
		}

		TTF_Font *operator[](const std::pair<std::u8string, int> &font)
		{
			const std::u8string key = font.first; // +std::to_string(font.second);
			if (fonts.count(key) == 0)
			{
				if (!(fonts[key] = TTF_OpenFont((char *)font.first.c_str(), font.second)))
				{
					SDL_Log("couldn't open font: %s %s", key.c_str(), TTF_GetError());
					fonts.erase(key);
					// TTF_OpenFontDPI()
					return nullptr;
				}
				else
				{
					// SDL_Log("NEW FONT: %s", key.c_str());
					// TTF_SetFontOutline(fonts[key], m_font_outline);
					// TTF_SetFontKerning(fonts[key], 0);
					// TTF_SetFontHinting(fonts[key], TTF_HINTING_MONO);
				}
			}
			return fonts[key];
		}

	private:
		std::unordered_map<std::u8string, TTF_Font *> fonts;
	};

	class FontSystem
	{
	public:
		FontSystem(const FontSystem &) = delete;

		FontSystem(const FontSystem &&) = delete;

		// SDL_Texture* uniTex;

		static FontSystem &Get()
		{
			static FontSystem instance;
			return instance;
		}

		FontSystem &setFontFile(const char *font_file) noexcept
		{
			this->m_font_attributes.font_file = font_file;
			return *this;
		}

		FontSystem &setFontSize(const uint8_t &font_size) noexcept
		{
			this->m_font_attributes.font_size = font_size;
			return *this;
		}

		FontSystem &setFontStyle(const FontStyle &font_style) noexcept
		{
			this->m_font_attributes.font_style = font_style;
			return *this;
		}

		FontSystem &setCustomFontStyle(const int &custom_font_style) noexcept
		{
			this->m_custom_fontstyle = custom_font_style;
			return *this;
		}

		FontSystem &setFontAttributes(const FontAttributes &font_attributes,
									  const int &custom_fontstyle = 0) noexcept
		{
			this->m_font_attributes = font_attributes;
			this->m_custom_fontstyle = custom_fontstyle;
			return *this;
		}

		FontSystem &setFontAttributes(const MemFont &_mem_ft, const FontStyle &ft_style_, const int &custom_fontstyle = 0) noexcept
		{
			this->m_font_attributes.setFontAttributes(FontAttributes{_mem_ft.font_name, ft_style_, static_cast<uint8_t>(_mem_ft.font_size)});
			this->m_custom_fontstyle = custom_fontstyle;
			return *this;
		}

		TTF_Font *getFont(const std::string &font_file, const int &font_size)
		{
			/*MemFont consolab_;
			consolab_.font_data = ff_consolab_ttf_data;
			consolab_.font_data_size = ff_consolab_ttf_len;
			consolab_.font_name = "consolab.ttf";
			consolab_.font_size = font_size;*/
			return fontStore[{font_file, font_size}];
			// return fontStore[consolab_];
		}

		TTF_Font *getFont(const MemFont &_mem_ft)
		{
			/*MemFont consolab_;
			consolab_.font_data = ff_consolab_ttf_data;
			consolab_.font_data_size = ff_consolab_ttf_len;
			consolab_.font_name = "consolab.ttf";
			consolab_.font_size = font_size;*/
			return fontStore[_mem_ft];
		}

		std::optional<UniqueTexture> genTextTextureUnique(SDL_Renderer *renderer, const char *text, const SDL_Color text_color)
		{
			if (!genTextCommon())
				return {};
			SDL_Surface *textSurf = TTF_RenderUTF8_Blended(m_font, text, text_color);
			// SDL_SetSurfaceBlendMode(textSurf, SDL_BLENDMODE_BLEND);
			if (!textSurf)
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
			auto result = CreateUniqueTextureFromSurface(renderer, textSurf);
			// SDL_SetTextureBlendMode(uniTex, SDL_BLENDMODE_BLEND);
			if (!result.get())
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
			// SDL_Rect messageRect = {(int) x, (int) y, textSurf->w, textSurf->h};
			// SDL_DestroySurface(textSurf);
			Async::GThreadPool.enqueue([](SDL_Surface *surface)
									   {SDL_DestroySurface(surface); surface = nullptr; },
									   textSurf);
			// textSurf = nullptr;
			return result;
		}

		std::optional<SharedTexture> genTextTextureShared(SDL_Renderer *renderer, const char *text, const SDL_Color text_color)
		{
			if (!genTextCommon())
				return {};
			SDL_Surface *textSurf = TTF_RenderUTF8_Blended(m_font, text, text_color);
			// SDL_SetSurfaceBlendMode(textSurf, SDL_BLENDMODE_BLEND);
			if (!textSurf)
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
			auto result = CreateSharedTextureFromSurface(renderer, textSurf);
			// SDL_SetTextureBlendMode(uniTex, SDL_BLENDMODE_BLEND);
			if (!result.get())
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
			// SDL_Rect messageRect = {(int) x, (int) y, textSurf->w, textSurf->h};
			// SDL_DestroySurface(textSurf);
			Async::GThreadPool.enqueue([](SDL_Surface *surface)
									   {SDL_DestroySurface(surface); surface = nullptr; },
									   textSurf);
			// textSurf = nullptr;
			return result;
		}

		std::optional<UniqueTexture> u8GenTextTextureUnique(SDL_Renderer *renderer, const char8_t *text, const SDL_Color text_color)
		{

			if (!genTextCommon())
				return {};

			SDL_Surface *textSurf = TTF_RenderUTF8_Blended(m_font, (char *)text, text_color);
			if (!textSurf)
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
			UniqueTexture result = CreateUniqueTextureFromSurface(renderer, textSurf);
			SDL_DestroySurface(textSurf);
			textSurf = nullptr;
			if (!result.get())
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", SDL_GetError());
				return {};
			}
			return result;
		}

	private:
		FontSystem()
		{
			this->m_font_attributes = FontAttributes{"", FontStyle::NORMAL, 255};
			m_font_outline = 0;
			m_custom_fontstyle = TTF_STYLE_NORMAL;
		}

		bool genTextCommon()
		{
			if (m_font_attributes.font_file.empty())
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "FontSystem Error: Invlaid/empty fontfile!");
				return false;
			}

			m_font = fontStore[{m_font_attributes.font_file, m_font_attributes.font_size}];
			if (m_font == nullptr)
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Error generating text texture: NULL FONT");
				return false;
			}

			switch (m_font_attributes.font_style)
			{
			case FontStyle::NORMAL:
				TTF_SetFontStyle(m_font, TTF_STYLE_NORMAL);
				break;
			case FontStyle::BOLD:
				TTF_SetFontStyle(m_font, TTF_STYLE_BOLD);
				break;
			case FontStyle::ITALIC:
				TTF_SetFontStyle(m_font, TTF_STYLE_ITALIC);
				break;
			case FontStyle::UNDERLINE:
				TTF_SetFontStyle(m_font, TTF_STYLE_UNDERLINE);
				break;
			case FontStyle::STRIKETHROUGH:
				TTF_SetFontStyle(m_font, TTF_STYLE_STRIKETHROUGH);
				break;
			case FontStyle::BOLD_UNDERLINE:
				TTF_SetFontStyle(m_font, TTF_STYLE_BOLD | TTF_STYLE_UNDERLINE);
				break;
			case FontStyle::BOLD_STRIKETHROUGH:
				TTF_SetFontStyle(m_font, TTF_STYLE_BOLD | TTF_STYLE_STRIKETHROUGH);
				break;
			case FontStyle::ITALIC_BOLD:
				TTF_SetFontStyle(m_font, TTF_STYLE_ITALIC | TTF_STYLE_BOLD);
				break;
			case FontStyle::ITALIC_UNDERLINE:
				TTF_SetFontStyle(m_font, TTF_STYLE_ITALIC | TTF_STYLE_UNDERLINE);
				break;
			case FontStyle::ITALIC_STRIKETHROUGH:
				TTF_SetFontStyle(m_font, TTF_STYLE_ITALIC | TTF_STYLE_STRIKETHROUGH);
				break;
			case FontStyle::CUSTOM:
				TTF_SetFontStyle(m_font, m_custom_fontstyle);
				break;
			default:
				TTF_SetFontStyle(m_font, TTF_STYLE_NORMAL);
				break;
			}

			return true;
		}

		TTF_Font *m_font;
		FontStore fontStore;
		FontAttributes m_font_attributes;
		int m_font_outline = 0;
		int m_custom_fontstyle;
	};

	struct TextViewAttributes
	{
		Font mem_font = Font::ConsolasBold;
		SDL_FRect rect = {0.f, 0.f, 0.f, 0.f};
		TextAttributes textAttributes = {"", {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff}};
		std::string fontFile;
		FontStyle fontStyle = FontStyle::NORMAL;
		Gravity gravity = Gravity::LEFT;
		int customFontstyle = TTF_STYLE_NORMAL;
		bool shrinkToFit = false;
	};
	class TextView : public Context
	{
	public:
		// using IView::rect;
		SDL_FRect txt_dest{}, rect{};
		std::size_t id = 0;

		TextView() {}

		TextView(Context *_context, const TextViewAttributes &tvAttr)
		{
			Context::setContext(_context);
			Build(tvAttr);
		}

		TextView &setContext(Context *context_) noexcept
		{
			Context::setContext(context_);
			return *this;
		}

		TextView &setId(const int &id_) noexcept
		{
			this->id = id_;
			return *this;
		}

		inline std::size_t getId() const noexcept
		{
			return this->id;
		}

		TextView &setPosX(const float &pos_x) noexcept
		{
			this->rect.x = pos_x;
			return *this;
		}

		TextView &setPosY(const float &pos_y) noexcept
		{
			this->rect.y = pos_y;
			return *this;
		}

		TextView &setPos(const float &pos_x, const float &pos_y) noexcept
		{
			this->rect.x = pos_x;
			this->rect.y = pos_y;
			return *this;
		}

		inline std::string getText() const noexcept
		{
			return this->textAttributes.text;
		}

		TextView &updateTextColor(const SDL_Color &bgColor, const SDL_Color &textColor)
		{
			textAttributes.bg_color = bgColor;
			textAttributes.text_color = textColor;
			// Build(dest, textAttributes, fontAttributes.font_file, fontAttributes.font_style, textGravity);
			CacheRenderTarget rTargetCache(renderer);
			auto genTex = FontSystem::Get().setFontAttributes(fontAttributes).genTextTextureUnique(renderer, textAttributes.text.c_str(), textAttributes.text_color);
			/*int tmp_w = 0, tmp_h = 0;
			SDL_QueryTexture(FontSystem::Get().uniTex, nullptr, nullptr, &tmp_w, &tmp_h);*/
			SDL_SetRenderTarget(renderer, this->stexture.get());
			renderClear(renderer, textAttributes.bg_color.r,
						textAttributes.bg_color.g, textAttributes.bg_color.b,
						textAttributes.bg_color.a);
			SDL_RenderTexture(renderer, genTex.value().get(), nullptr, nullptr);
			rTargetCache.release(renderer);
			return *this;
		}

		TextView &Build(const TextViewAttributes &tvAttr)
		{
			CacheRenderTarget rTargetCache(renderer);
			this->rect = {0.f, 0.f, tvAttr.rect.w, tvAttr.rect.h};
			if (stexture.get() != nullptr)
			{
				stexture.reset();
				stexture = nullptr;
			}
			shrinkToFit = tvAttr.shrinkToFit;
			// u8text = std::u8string(text_attributes.text);

			int fsize = tvAttr.rect.h;
			if (fsize > 255)
				fsize = 255;
			textGravity = tvAttr.gravity;
			// int btmp_w = 0, btmp_h = 0;
			// btmp_h = fsize;
			// btmp_w = dest_.h * text_attributes.text.size();
			////TTF_SizeText(FontSystem::Get().getFont(font_file, fsize), text_attributes.text.c_str(), &btmp_w, &btmp_h);
			// SDL_Log("BQ_W: %d BQ_H: %d", btmp_w, btmp_h);
			textAttributes = tvAttr.textAttributes;
			fontAttributes = {tvAttr.fontFile, tvAttr.fontStyle, static_cast<Uint8>(fsize)};
			TTF_Font *tmpFont = nullptr;

			if (fontAttributes.font_file.empty())
			{
				Fonts[tvAttr.mem_font]->font_size = fontAttributes.font_size;
				fontAttributes.font_file = Fonts[tvAttr.mem_font]->font_name;
				tmpFont = FontSystem::Get().getFont(*Fonts[tvAttr.mem_font]);
			}

			auto genTex = FontSystem::Get().setFontAttributes(fontAttributes).genTextTextureUnique(renderer, textAttributes.text.c_str(), textAttributes.text_color);
			if (genTex.has_value())
			{
				int tmp_w = 0, tmp_h = 0;
				SDL_QueryTexture(genTex.value().get(), nullptr, nullptr, &tmp_w, &tmp_h);
				// SDL_Log("AQ_W: %d AQ_H: %d", tmp_w, tmp_h);
				if (tmp_w < rect.w)
				{
					// if(shrinkToFit)dest.w = tmp_w;
					rect.w = tmp_w;
					if (tvAttr.gravity == Gravity::CENTER && !shrinkToFit)
					{
						rect.x = (tvAttr.rect.w - tmp_w) / 2.f;
					}
				}
				if (tmp_h < rect.h)
					rect.y = (tvAttr.rect.h - tmp_h) / 2.f, rect.h = tmp_h;

				SDL_FRect src_txt = {0.f, 0.f, rect.w, tmp_h};
				stexture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
											   SDL_TEXTUREACCESS_TARGET, shrinkToFit ? rect.w : tvAttr.rect.w,
											   shrinkToFit ? rect.h : tvAttr.rect.h);
				SDL_SetTextureBlendMode(this->stexture.get(), SDL_BLENDMODE_BLEND);
				SDL_SetRenderTarget(renderer, this->stexture.get());
				renderClear(renderer, textAttributes.bg_color.r,
							textAttributes.bg_color.g, textAttributes.bg_color.b,
							textAttributes.bg_color.a);
				SDL_RenderTexture(renderer, genTex.value().get(), &src_txt, &rect);
				if (!shrinkToFit)
					rect.x = tvAttr.rect.x, rect.w = tvAttr.rect.w;
				else
					rect.x += tvAttr.rect.x;
				rect.y += tvAttr.rect.y;
			}
			// dest = dest_;
			rTargetCache.release(renderer);
			return *this;
		}

		const TextView &Draw() const
		{
			if (stexture.get() == nullptr)
			{
				// SDL_Log("EMPTY_TEXTVIEW");
				return *this;
			}
			/*SDL_SetRenderDrawColor(renderer, 0xc8, 0, 0, 0xff);
			SDL_RenderFillRect(renderer, &dest);*/
			SDL_RenderTexture(renderer, stexture.get(), nullptr, &rect);
			return *this;
		}

	protected:
		SharedTexture stexture;
		bool shrinkToFit = false;
		// std::u8string u8text;
		TextAttributes textAttributes;
		FontAttributes fontAttributes;
		Gravity textGravity;
	};

	class TextSlider : public Context
	{
	public:
		SDL_FRect dest;

	public:
		TextSlider() = default;

		TextSlider(Context *_context, const SDL_FRect &dest_, const TextAttributes &text_attriutes,
				   const FontAttributes &font_attributes)
		{
			Context::setContext(_context);
			contextIsSet = true;
			Build(dest_, text_attriutes, font_attributes);
		}

		TextSlider &setContext(Context *context_)
		{
			Context::setContext(context_);
			contextIsSet = true;
			return *this;
		}

		TextSlider &setSlideDelay(const MilliSec<int> &ms_delay)
		{
			this->tmSlideDelay = ms_delay.get();
			return *this;
		}

		TextSlider &configureSidesShadow(const SDL_Color &shadow_color, const int &percLength)
		{
			this->sidesShadowColor = shadow_color;
			this->sideShadowLenght = percLength;
			showSidesShadow = true;
			return *this;
		}

		TextSlider &Build(const SDL_FRect &dest_, const TextAttributes &text_attributes, const FontAttributes &font_attributes)
		{
			animSwitchAdaptiveVsyncHD.setAdaptiveVsync(adaptiveVsync);
			slideAdaptiveVsyncHD.setAdaptiveVsync(adaptiveVsync);
			if (!contextIsSet)
			{
				SDL_Log("Couldn't create TextSlider: Context is null/wasn't set");
				return *this;
			}

			if (sliderTexture.get() != nullptr)
				sliderTexture.reset();

			dest = dest_;

			textAttributes = text_attributes;
			fontAttributes = font_attributes;
			fontAttributes.font_size = dest.h;

			CacheRenderTarget rTargetCache(renderer);

			sliderTexture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, dest_.w,
												dest_.h);
			SDL_SetTextureBlendMode(this->sliderTexture.get(), SDL_BLENDMODE_BLEND);
			// rTargetCache = SDL_GetRenderTarget(renderer);
			SDL_SetRenderTarget(renderer, sliderTexture.get());
			renderClear(renderer, textAttributes.bg_color.r, textAttributes.bg_color.g, textAttributes.bg_color.b,
						textAttributes.bg_color.a);

			// oldTexture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, Width(dest_.w), Height(dest_.h));
			// SDL_SetTextureBlendMode(this->oldTexture.get(), SDL_BLENDMODE_BLEND);
			////rTargetCache = SDL_GetRenderTarget(renderer);
			// SDL_SetRenderTarget(renderer, oldTexture.get());
			// renderClear(renderer, textAttributes.bg_color.r, textAttributes.bg_color.g, textAttributes.bg_color.b, textAttributes.bg_color.a);
			//
			updateTextures();
			rTargetCache.release(renderer);
			tm_start = SDL_GetTicks();
			tm_last = SDL_GetTicks();
			vi = pv->to_cust(5.f, dest.h);
			return *this;
		}

		TextSlider &updateText(const char *newText)
		{
			animDestA = cache_newDest;
			textAttributes.setText(newText);
			slide = true;
			if (oldTexture.get() != nullptr)
				oldTexture.reset();
			/*SDL_SetRenderTarget(renderer, newTexture);
			renderClear(0, 0, 0, 0);
			SDL_RenderTexture(renderer, newTexture, nullptr, &animDestA);
			*/
			oldTexture = std::move(newTexture);
			CacheRenderTarget rTargetCache(renderer);
			SDL_SetRenderTarget(renderer, sliderTexture.get());
			renderClear(renderer, textAttributes.bg_color.r, textAttributes.bg_color.g, textAttributes.bg_color.b,
						textAttributes.bg_color.a);
			updateTextures();
			animDestB = {newDest.x, newDest.y + newDest.h, newDest.w, newDest.h};
			animateSwitch = true;
			rTargetCache.release(renderer);
			if (animateSwitch)
				animSwitchAdaptiveVsyncHD.startRedrawSession();
			return *this;
		}

		const TextSlider &Draw()
		{
			if (animateSwitch)
			{
				if (animDestA.y <= -animDestA.h)
					animateSwitch = false, animSwitchAdaptiveVsyncHD.stopRedrawSession(), animDestB.y = newDest.y, tm_start = SDL_GetTicks(), tm_last = SDL_GetTicks();
				CacheRenderTarget rTargetCache(renderer);
				SDL_SetRenderTarget(renderer, sliderTexture.get());
				renderClear(renderer, textAttributes.bg_color.r, textAttributes.bg_color.g, textAttributes.bg_color.b,
							textAttributes.bg_color.a);
				SDL_RenderTexture(renderer, oldTexture.get(), &source_a, &animDestA);
				SDL_RenderTexture(renderer, newTexture.get(), &source_a, &animDestB);
				rTargetCache.release(renderer);
				animDestA.y -= vi;
				if (animDestB.y > newDest.y)
					animDestB.y -= vi;
			}
			else
			{
				if (slide)
				{
					// updateSlide();
					tm_now = SDL_GetTicks();
					if (tm_now - tm_start > tmSlideDelay)
					{
						updateSlide();
					}
					else
					{
						tm_last = SDL_GetTicks();
					}
				}
			}
			SDL_RenderTexture(renderer, sliderTexture.get(), nullptr, &dest);
			if (showSidesShadow && slide && (tm_now - tm_start) > tmSlideDelay)
			{
				SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
				const int len = pv->to_cust(sideShadowLenght, dest.w);
				/// TODO: cap perc_ to 255 using std::max
				const int alpha_ratio = (int)(255 / len);
				uint8_t alpha = 255;
				for (int i = 0; i < len; ++i)
				{
					alpha -= alpha_ratio;
					SDL_SetRenderDrawColor(renderer, sidesShadowColor.r, sidesShadowColor.g, sidesShadowColor.b, alpha);
					SDL_RenderLine(renderer, (float)dest.x + i, dest.y, (float)dest.x + i, dest.y + dest.h);
					SDL_RenderLine(renderer, dest.x + dest.w - i, dest.y, dest.x + dest.w - i, dest.y + dest.h);
				}
			}

			return *this;
		}

	private:
		void updateTextures()
		{
			// rTargetCache = SDL_GetRenderTarget(renderer);
			if (!textAttributes.text.empty())
			{
				auto textTex = FontSystem::Get().setFontAttributes(fontAttributes).genTextTextureUnique(renderer, textAttributes.text.c_str(), textAttributes.text_color);
				int tmp_w = 0, tmp_h = 0;
				SDL_QueryTexture(textTex.value().get(), nullptr, nullptr, &tmp_w, &tmp_h);
				newTw = (float)tmp_w, newTh = (float)tmp_h;
				if (newTexture.get() != nullptr)
					newTexture.reset();
				newTexture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, newTw, newTh);
				SDL_SetTextureBlendMode(this->newTexture.get(), SDL_BLENDMODE_BLEND);
				SDL_SetRenderTarget(renderer, newTexture.get());
				renderClear(renderer, 0, 0, 0, 0);
				SDL_RenderTexture(renderer, textTex.value().get(), nullptr, nullptr);

				SDL_SetRenderTarget(renderer, sliderTexture.get());
				SDL_SetTextureBlendMode(this->sliderTexture.get(), SDL_BLENDMODE_BLEND);
				renderClear(renderer, textAttributes.bg_color.r, textAttributes.bg_color.g, textAttributes.bg_color.b,
							textAttributes.bg_color.a);

				newDest = {0.f, 0.f, dest.w, dest.h};
				source_a = {0.f, 0.f, dest.w, newTh};
				source_b = {0.f, 0.f, dest.w, newTh};
				newDestb = {dest.w, 0.f, dest.w, dest.h};
				if (newTw < newDest.w)
					newDest.x = newDest.x + ((dest.w - newTw) / 2.f), newDest.w = newTw, slide = false;
				if (newTh < newDest.h)
				{
					newDest.y = (dest.h - newTh) / 2.f;
					newDest.h = newTh;
					newDestb.y = (dest.h - newTh) / 2.f;
					newDestb.h = newTh;
				}
				cache_source_a = source_a;
				cache_newDest = newDest;
				cache_source_b = source_b;
				cache_newDestb = newDestb;
				SDL_RenderTexture(renderer, newTexture.get(), &source_a, &newDest);
			}
			else
				slide = false;
			tm_start = SDL_GetTicks();
			tm_last = SDL_GetTicks();
			if (slide)
				slideAdaptiveVsyncHD.startRedrawSession();
			else
				slideAdaptiveVsyncHD.stopRedrawSession();
		}

		void updateSlide()
		{
			CacheRenderTarget rTargetCache(renderer);
			SDL_SetRenderTarget(renderer, sliderTexture.get());
			renderClear(renderer, textAttributes.bg_color.r, textAttributes.bg_color.g, textAttributes.bg_color.b,
						textAttributes.bg_color.a);
			// tm_now = SDL_GetTicks();
			float dt = (float)(SDL_GetTicks() - tm_last);
			// std::cout << "ts: " << dt << std::endl;
			dt = dt * 0.1f;
			vel = (int)(std::floorf(dt));
			tm_last = SDL_GetTicks();
			// vel = 1;
			source_a.x += vel;
			if (source_a.x + source_a.w > newTw)
				newDest.w -= vel;
			if (source_a.x + source_a.w > newTw + (dest.w / 2))
			{
				newDestb.x -= vel;
				// SDL_Log("this");
				// source_b.x++;
				// std::cout << ".";
				// SDL_SetRenderDrawColor(renderer, 0xff, 0, 0, 0xff);
				// SDL_RenderFillRect(renderer, &newDestb);
				SDL_RenderTexture(renderer, newTexture.get(), &source_b, &newDestb);
			}
			if (newDestb.x <= 0)
			{
				source_a = cache_source_a, newDest = cache_newDest, tm_start = SDL_GetTicks(), tm_last = SDL_GetTicks();
				source_b = cache_source_b, newDestb = cache_newDestb;
			}
			SDL_RenderTexture(renderer, newTexture.get(), &source_a, &newDest);
			rTargetCache.release(renderer);
		}

	private:
		bool contextIsSet = false, slide = true, showSidesShadow = false, animateSwitch = false;
		SharedTexture oldTexture, newTexture, sliderTexture;
		TextAttributes textAttributes;
		FontAttributes fontAttributes;

		SDL_FRect animDestA, animDestB, newDest, newDestb, cache_newDestb, cache_newDest, source_a, source_b, cache_source_a, cache_source_b;
		Uint32 tm_start = 0, tm_last = 0, tm_now = 0, tmSlideDelay = 2000;
		float newTw = 0, newTh = 0, sideShadowLenght = 0, vi = 2, hi = 2;
		SDL_Color sidesShadowColor;
		int vel = 1;
		AdaptiveVsyncHandler animSwitchAdaptiveVsyncHD, slideAdaptiveVsyncHD;
	};

	struct RunningTextAttributes
	{
		SDL_FRect rect{0.f, 0.f, 0.f, 0.f};
		SDL_Color text_color{255, 255, 255, 255};
		SDL_Color bg_color{0, 0, 0, 0};
		Font mem_font = Font::ConsolasBold;
		FontStyle font_style = FontStyle::NORMAL;
		std::string font_file;
		uint32_t pause_duration = 3000;
		uint32_t speed = 5000;
		// transition speed in 0.f%-100.f%
		float transition_speed = 2.f;
	};

	class RunningText : public Context, IView
	{
	public:
		using IView::bounds;
		using IView::getView;

	public:
	public:
		RunningText() = default;
		RunningText(Context *_context, const RunningTextAttributes &_attr)
		{
			Build(_context, _attr);
		}

		RunningText &Build(Context *_context, const RunningTextAttributes &_attr)
		{
			setContext(_context);
			adaptiveVsyncHD.setAdaptiveVsync(adaptiveVsync);
			attr = _attr;
			bounds = _attr.rect;
			attr.transition_speed = DisplayInfo::Get().to_cust(_attr.transition_speed, bounds.h);
			step_tm = (float)attr.speed / bounds.w;
			texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, bounds.w, bounds.h);
			SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
			return *this;
		}

		bool handleEvent() override
		{
			bool result = false;
			return result;
		}

		void updatePosBy(float _dx, float _dy) override
		{
			bounds.x += _dx;
			bounds.y += _dy;
		}

		void updateText(const std::string &_text)
		{
			if (_text.empty())
				return;
			is_running = false;
			is_centered = false;

			CacheRenderTarget crt_(renderer);
			SDL_SetRenderTarget(renderer, texture.get());
			renderClear(renderer, attr.bg_color.r, attr.bg_color.g, attr.bg_color.b, attr.bg_color.a);

			FontAttributes fontAttrb = {attr.font_file, attr.font_style, static_cast<uint8_t>(bounds.h)};
			TTF_Font *tmpFont = nullptr;
			if (fontAttrb.font_file.empty())
			{
				Fonts[attr.mem_font]->font_size = fontAttrb.font_size;
				fontAttrb.font_file = Fonts[attr.mem_font]->font_name;
				tmpFont = FontSystem::Get().getFont(*Fonts[attr.mem_font]);
			}

			const auto fa_ = static_cast<float>(TTF_FontAscent(tmpFont));
			const auto fd_ = static_cast<float>(TTF_FontDescent(tmpFont));

			FontSystem::Get().setFontAttributes(std::move(fontAttrb));
			text_texture = FontSystem::Get().genTextTextureShared(renderer, _text.c_str(), attr.text_color).value();
			int tw = 0, th = 0;
			SDL_QueryTexture(text_texture.get(), nullptr, nullptr, &tw, &th);
			SDL_FRect dst{0.f, 0.f, bounds.w, bounds.h};
			dst.w = static_cast<float>(tw);
			if (static_cast<float>(th) > dst.h)
				dst.y += fd_, dst.h = static_cast<float>(th);
			if (dst.w < bounds.w)
				is_centered = true, dst.x += ((bounds.w - dst.w) / 2.f);
			SDL_RenderTexture(renderer, text_texture.get(), nullptr, &dst);
			txt_rect = dst;
			txt_rect2 = dst;
			txt_rect2.x = dst.w + DisplayInfo::Get().to_cust(50.f, bounds.w);
			cache_txt_rect2_x = txt_rect2.x;
			crt_.release(renderer);
			tm_last_pause = SDL_GetTicks();
			tm_last_update = SDL_GetTicks();
			adaptiveVsyncHD.startRedrawSession();
		}

		void Draw() override
		{
			if (not is_centered)
			{
				const auto now = SDL_GetTicks();
				if (now - tm_last_pause > attr.pause_duration and not is_running)
				{
					is_running = true;
					tm_last_update = SDL_GetTicks();
				}
				if (is_running)
					update();
			}
			SDL_RenderTexture(renderer, texture.get(), nullptr, &bounds);
		}

	private:
		void update()
		{
			CacheRenderTarget crt_(renderer);
			SDL_SetRenderTarget(renderer, texture.get());
			renderClear(renderer, attr.bg_color.r, attr.bg_color.g, attr.bg_color.b, attr.bg_color.a);
			const auto df = static_cast<float>(SDL_GetTicks() - tm_last_update);
			if (df >= step_tm)
			{
				float num_steps = df / (float)step_tm;
				txt_rect.x -= num_steps;
				txt_rect2.x -= num_steps;
				tm_last_update = SDL_GetTicks();
			}
			if (txt_rect2.x < 0.f)
			{
				txt_rect.x = 0.f;
				txt_rect2.x = cache_txt_rect2_x;
				is_running = false;
				tm_last_pause = SDL_GetTicks();
				tm_last_update = SDL_GetTicks();
			}
			SDL_RenderTexture(renderer, text_texture.get(), nullptr, &txt_rect);
			SDL_RenderTexture(renderer, text_texture.get(), nullptr, &txt_rect2);
			crt_.release(renderer);
		}

	private:
		RunningTextAttributes attr;
		SharedTexture texture;
		SharedTexture text_texture;
		SDL_FRect txt_rect{0.f, 0.f, 0.f, 0.f};
		SDL_FRect txt_rect2{0.f, 0.f, 0.f, 0.f};
		bool is_running = false;
		bool is_centered = false;
		uint32_t tm_last_pause = 0;
		float cache_txt_rect2_x = 0.f;
		uint32_t step_tm = 0;
		uint32_t tm_last_update = 0;
		AdaptiveVsyncHandler adaptiveVsyncHD;
	};

	class TextNavBarBuilder;

	class TextNavBar : Context
	{
	public:
		TextNavBar() = default;
		friend class TextNavBarBuilder;
		SDL_FRect rect;
		static TextNavBarBuilder Builder(Context *context);

		TextNavBar &handleEvent(const SDL_Event &event)
		{
			if (event.type == SDL_EVENT_FINGER_DOWN)
			{
			}
			else if (event.type == SDL_EVENT_FINGER_MOTION)
			{
			}
			else if (event.type == SDL_EVENT_FINGER_UP)
			{
				const SDL_FPoint cf = {(event.tfinger.x * DisplayInfo::Get().RenderW) - rect.x,
									   (event.tfinger.y * DisplayInfo::Get().RenderH) - rect.y};
				std::size_t tmpSelectedTextViewIndex;
				const bool found = std::any_of(textViews.begin(), textViews.end(), [&](const TextView &textView)
											   {
					if (SDL_PointInRectFloat(&cf, &textView.rect)) {
						tmpSelectedTextViewIndex = textView.getId();
						return true;
					}
					else return false; });

				if (found && tmpSelectedTextViewIndex != selectedTextViewIndex)
				{
					UPDATE_SELECTED_TEXTVIEW = true;
					textViews[selectedTextViewIndex].updateTextColor({0, 0, 0, 0}, unselectedTextColor);
					textViews[tmpSelectedTextViewIndex].updateTextColor({0, 0, 0, 0}, selectedTextColor);
					if (is_scrollable)
					{
						float distance_to_move = get_distance_to_move(tmpSelectedTextViewIndex);
						if (distance_to_move < 0.f)
							ACTN_MINUS = true;
						else
							ACTN_MINUS = false;
						animInterpolator.startWithDistance(std::fabs(distance_to_move));
					}
					selectedTextViewIndex = tmpSelectedTextViewIndex;
				}
			}
			return *this;
		}

		void udpdateSelectedText(const uint32_t &_index)
		{
			if (_index > textViews.size())
			{
				SDL_Log("updateSelectedText called with out of range index. index: %d, textViews size: %u", _index, textViews.size());
				return;
			}
			if (_index != selectedTextViewIndex)
			{
				if (onSelectedTextViewUpdateCallback != nullptr)
					onSelectedTextViewUpdateCallback(textViews[selectedTextViewIndex]);
				textViews[selectedTextViewIndex].updateTextColor({0, 0, 0, 0}, unselectedTextColor);
				textViews[_index].updateTextColor({0, 0, 0, 0}, selectedTextColor);
				selectedTextViewIndex = _index;
				const float sel_x = (rect.w / 2.f) - pv->to_cust(20.f, rect.w);
				const float distanceMoved = sel_x - textViews[_index].rect.x;

				SDL_Log("DM: %f", distanceMoved);
				std::for_each(textViews.begin(), textViews.end(), [&](TextView &textView)
							  { textView.rect.x += distanceMoved; });
			}
			if (onSelectedTextViewUpdateCallback != nullptr)
				onSelectedTextViewUpdateCallback(textViews[selectedTextViewIndex]);
			UPDATE_SELECTED_TEXTVIEW = true;
		}

		TextNavBar &Draw()
		{
			if (UPDATE_SELECTED_TEXTVIEW)
			{
				if (animInterpolator.isAnimating())
				{
					animInterpolator.update();
					dy = static_cast<int>(animInterpolator.getValue());
					dy = ACTN_MINUS ? (0 - dy) : dy;
				}
				else
				{
					UPDATE_SELECTED_TEXTVIEW = false;
					ACTN_MINUS = false;
					if (onSelectedTextViewUpdateCallback != nullptr)
						onSelectedTextViewUpdateCallback(textViews[selectedTextViewIndex]);
				}
				CacheRenderTarget rTargetCache(renderer);
				SDL_SetRenderTarget(renderer, texture.get());
				SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
				renderClear(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
				for (auto &textview : textViews)
				{
					if (UPDATE_SELECTED_TEXTVIEW)
						textview.rect.x += dy;
					textview.Draw();
				}
				dy = 0.f;
				rTargetCache.release(renderer);
			}
			SDL_RenderTexture(renderer, texture.get(), nullptr, &rect);
			return *this;
		}

	private:
		TextNavBar &registerOnSelectedTextViewUpdateCallback(std::function<void(const TextView &)> _onSelectedTextViewUpdateCallback)
		{
			onSelectedTextViewUpdateCallback = _onSelectedTextViewUpdateCallback;
			return *this;
		}

		TextNavBar &Build(const std::vector<std::string> &_texts, const std::string &_fontFile,
						  const FontStyle &_fontStyle, const std::size_t &_defaultSelectedTextIndex = 0)
		{
			// if (_dest.w < 1 || _dest.h < 1)return *this;
			// Context::setContext(_context);
			/*rect = _dest;
			bgColor = _bgColor;
			selectedTextColor = _selected_text_color;
			unselectedTextColor = _unselected_text_color;*/

			spacing = pv->to_cust(5.f, rect.w);
			float i = 0.f, prev_tv_width = 0.f;
			for (const auto &text : _texts)
			{
				textViews.emplace_back();
				textViews.back().setContext(getContext()).setId(i).Build({.rect = {(i * spacing) + prev_tv_width, 0, rect.w /*to_cust(40.f, _dest.w)*/, rect.h}, .textAttributes = {text, i == _defaultSelectedTextIndex ? selectedTextColor : unselectedTextColor, {0, 0, 0, 0}}, .fontFile = _fontFile, .fontStyle = _fontStyle, .shrinkToFit = true});
				prev_tv_width += textViews.back().rect.w;
				++i;
			}

			// center the textviews
			/// TODO: a better approach would be to add up the two dx's so that theycan balance and use a single for each instead
			if (textViews.back().rect.x + textViews.back().rect.w > rect.w)
			{
				float dd = get_distance_to_move(_defaultSelectedTextIndex);
				std::for_each(textViews.begin(), textViews.end(), [&dd](TextView &textView)
							  { textView.rect.x += dd; });
				is_scrollable = true;
			}
			else
			{
				float dx = rect.w - (textViews.back().rect.x + textViews.back().rect.w);
				dx /= 2.f;
				std::for_each(textViews.begin(), textViews.end(), [&dx](TextView &textView)
							  { textView.rect.x += dx; });
			}
			selectedTextViewIndex = std::clamp(_defaultSelectedTextIndex, std::size_t(0), _texts.size());

			CacheRenderTarget rTargetCache(renderer);
			texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, rect.w, rect.h);
			SDL_SetRenderTarget(renderer, texture.get());
			SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
			renderClear(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
			// for (const auto& text : textViews) { text.Draw(); }
			rTargetCache.release(renderer);
			UPDATE_SELECTED_TEXTVIEW = true;
			return *this;
		}

		float get_distance_to_move(const int &tmpSelectedTextViewIndex)
		{
			float distance_to_move = 0.f;
			if (tmpSelectedTextViewIndex > selectedTextViewIndex)
			{
				if (tmpSelectedTextViewIndex + 1 < textViews.size())
				{
					if (textViews[tmpSelectedTextViewIndex + 1].rect.x + textViews[tmpSelectedTextViewIndex + 1].rect.w > rect.w - spacing)
					{
						distance_to_move = (textViews[tmpSelectedTextViewIndex + 1].rect.x + textViews[tmpSelectedTextViewIndex + 1].rect.w) - (rect.w - spacing);
						distance_to_move = 0.f - distance_to_move;
					}
				}
				else
				{
					if (textViews[tmpSelectedTextViewIndex].rect.x + textViews[tmpSelectedTextViewIndex].rect.w > rect.w - spacing)
					{
						distance_to_move = (textViews[tmpSelectedTextViewIndex].rect.x + textViews[tmpSelectedTextViewIndex].rect.w) - (rect.w - spacing);
						distance_to_move = 0.f - distance_to_move;
					}
				}
			}
			else if (tmpSelectedTextViewIndex < selectedTextViewIndex)
			{
				if (tmpSelectedTextViewIndex - 1 >= 0)
				{
					if (textViews[tmpSelectedTextViewIndex - 1].rect.x < spacing)
					{
						distance_to_move = std::fabs(textViews[tmpSelectedTextViewIndex - 1].rect.x) + spacing;
					}
				}
				else
				{
					if (textViews[tmpSelectedTextViewIndex].rect.x < 0.f)
					{
						distance_to_move = std::fabs(textViews[tmpSelectedTextViewIndex].rect.x) + spacing;
					}
				}
			}
			return distance_to_move;
		}

	private:
		bool UPDATE_SELECTED_TEXTVIEW = false, ACTN_MINUS = false;
		float dy = 0.f, spacing = 0.f;
		Interpolator animInterpolator;
		// int tmpSelectedTextViewIndex = -1;
		SDL_Color bgColor = {0, 0, 0, 0};
		SDL_Color selectedTextBgColor, selectedTextColor = {0xff, 0xff, 0xff, 0xff};
		SDL_Color unselectedTextBgColor, unselectedTextColor = {0xff, 0xff, 0xff, 127};
		size_t selectedTextViewIndex = 0;
		std::vector<TextView> textViews;
		std::shared_ptr<SDL_Texture> texture;
		std::function<void(const TextView &)> onSelectedTextViewUpdateCallback = nullptr;
		bool is_scrollable = false;
	};

	class TextNavBarBuilder
	{
	public:
		TextNavBarBuilder(Context *context)
		{
			textNavBar.setContext(context);
		}

		TextNavBarBuilder &setRect(SDL_FRect _rect)
		{
			textNavBar.rect = _rect;
			return *this;
		}

		TextNavBarBuilder &setDefaultTextIndex(const int &_dsv)
		{
			dsv = _dsv;
			return *this;
		}

		TextNavBarBuilder &addTexts(const std::vector<std::string> &_texts)
		{
			texts = _texts;
			return *this;
		}

		TextNavBarBuilder &setBgColor(SDL_Color _bgColor)
		{
			textNavBar.bgColor = _bgColor;
			return *this;
		}

		TextNavBarBuilder &setFontFile(const std::string &_fontFile)
		{
			fontFile = _fontFile;
			return *this;
		}

		TextNavBarBuilder &setFontStyle(const FontStyle &_fontStyle)
		{
			fontStyle = _fontStyle;
			return *this;
		}

		TextNavBarBuilder &setSelectedTextColor(SDL_Color _selectedTextColor)
		{
			textNavBar.selectedTextColor = _selectedTextColor;
			return *this;
		}

		TextNavBarBuilder &setUnselectedTextColor(SDL_Color _unselectedTextColor)
		{
			textNavBar.unselectedTextColor = _unselectedTextColor;
			return *this;
		}

		TextNavBarBuilder &registerOnSelectedTextViewUpdate(std::function<void(const TextView &)> _onSelectedTextViewUpdateCallback)
		{
			textNavBar.onSelectedTextViewUpdateCallback = _onSelectedTextViewUpdateCallback;
			return *this;
		}

		operator TextNavBar &&()
		{
			textNavBar.Build(texts, fontFile, fontStyle, dsv);
			return std::move(textNavBar);
		}

	private:
		TextNavBar textNavBar;
		std::vector<std::string> texts;
		FontStyle fontStyle = FontStyle::NORMAL;
		std::string fontFile;
		int dsv = 0;
	};

	TextNavBarBuilder TextNavBar::Builder(Context *context)
	{
		return TextNavBarBuilder(context);
	}

	class UIWidget
	{
	public:
		SharedTexture texture;
		int id, transition;
		std::string s_id;
		SDL_Color bg_color, border_color;
		bool isButton, held;
		SDL_FRect dest;
		EdgeType edgeType;

		UIWidget()
		{
			id = transition = 0;
			bg_color = border_color = {0, 0, 0, 0};
			isButton = held = false;
			dest = {0.f, 0.f, 0.f, 0.f};
			edgeType = EdgeType::RECT;
		}

		void Draw(SDL_Renderer *renderer) const
		{
			SDL_RenderTexture(renderer, this->texture.get(), nullptr, &dest);
		}

		/// TODO: add a std::function parameter that will be called if true
		template <typename T>
		bool onClick(T x, T y, unsigned short axis = 0)
		{
			if (axis == 0)
			{
				if (x > dest.x && x < (dest.w + dest.w))
					if (y > dest.y && y < (dest.h + dest.y))
						return true;
			}
			else if (axis == 1 /*x-axis only*/)
			{
				if (x > dest.x && x < (dest.w + dest.w))
					return true;
			}
			else if (axis == 2 /*y-axis only*/)
			{
				if (y > dest.y && y < (dest.h + dest.y))
					return true;
			}
			return false;
		}
	};

	class ImageStore
	{
	private:
		// some data
	public:
		ImageStore() {}

		ImageStore(char *data)
		{
			// public constructor
		}

		void getImageById();
	};

	class TextureStore : public Context
	{
	public:
		TextureStore() {}

		TextureStore(const TextureStore &) = delete;

		TextureStore(const TextureStore &&) = delete;

		static TextureStore &Get()
		{
			static TextureStore instance;
			return instance;
		}

		~TextureStore()
		{
			reset();
		}

		void reset()
		{
			for (auto &_texture : textures_)
			{
				SDL_DestroyTexture(_texture.second);
				_texture.second = nullptr;
			}
			textures_.clear();
		}

		SDL_Texture *addToStore(const std::string &_key, SDL_Texture *_texture)
		{
			if (textures_.count(_key) == 0)
			{
				textures_[_key] = _texture;
			}
			return textures_[_key];
		}

		void erase(const std::string &_key)
		{
			if (textures_.count(_key) == 0)
				textures_.erase(_key);
		}

		SDL_Texture *operator[](const std::string &_path)
		{
			if (textures_.count(_path) == 0)
			{
				if (!(textures_[_path] = IMG_LoadTexture(renderer, _path.c_str())))
				{
					SDL_Log("%s", IMG_GetError());
					textures_.erase(_path);
					return nullptr;
				}
				// else {
				//     //SDL_Log("NEW FONT: %s", key.c_str());
				// }
			}
			return textures_[_path];
		}

	private:
		std::unordered_map<std::string, SDL_Texture *> textures_;
	};

	class RadialRect : public UIWidget, public Context
	{
	public:
		int border_sz = 0;
		int rad_ = 0;
		SDL_Color inner_color = {0};
		bool has_fading_border = false;

		RadialRect() = default;

		RadialRect(Context *context_, SDL_FRect a_dest,
				   const int &rad, const SDL_Color &color,
				   const SDL_Color &border_col = {}, const int &border_size = 0)
		{
			Context::setContext(context_);
			Build(a_dest, rad, color, border_col, border_size);
		}

		RadialRect &setContext(Context *context_)
		{
			Context::setContext(context_);
			return *this;
		}

		void Build(SDL_FRect a_dest,
				   const float &rad, const SDL_Color &color,
				   const SDL_Color &border_col = {}, const int &border_size = 0)
		{
			this->dest = a_dest;
			this->border_sz = border_size;
			this->rad_ = rad;
			this->inner_color = color, this->border_color = border_col;
			if (dest.w < 1 || dest.h < 1)
			{
				SDL_Log("Null RadialRect due to invlaid size params");
				return;
			}
			DestroyTextureSafe(this->texture.get());
			this->texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
												SDL_TEXTUREACCESS_TARGET,
												this->dest.w + (border_size * 2),
												this->dest.h + (border_size * 2));
			SDL_SetTextureBlendMode(this->texture.get(), SDL_BLENDMODE_BLEND);
			CacheRenderColor(renderer);
			updateBorderColor(border_col);
			updateInnerColor(color);
			RestoreCachedRenderColor(renderer);
			/*rTargetCache = SDL_GetRenderTarget(renderer);
			SDL_SetRenderTarget(renderer, this->texture.get());
			renderClear(renderer, 0, 0, 0, 0);
			fillRoundedRect(renderer, {0, 0, this->dest.w + (border_size * 2),
									   this->dest.h + (border_size * 2)},
							rad, border_col);
			fillRoundedRect(renderer,
							{border_size, border_size, this->dest.w - 1, this->dest.h - 1},
							rad, color);
			SDL_SetRenderTarget(renderer, rTargetCache);*/
			/*this->dest.w += (border_size);
			this->dest.h += (border_size);*/
		}

		void resize(const float &_w, const float &_h)
		{
			this->dest.w = _w, this->dest.h = _h;
			this->Build(dest, rad_, inner_color, border_color, border_sz);
		}

		void updateInnerColor(const SDL_Color &color)
		{
			inner_color = color;
			CacheRenderTarget rTargetCache(renderer);
			SDL_SetRenderTarget(renderer, this->texture.get());
			renderClear(renderer, 0, 0, 0, 0);
			fillRoundedRectF(renderer,
							 {(float)border_sz, (float)border_sz, this->dest.w, this->dest.h}, rad_,
							 inner_color);
			rTargetCache.release(renderer);
		}

		void updateBorderColor(const SDL_Color &_bordercolor)
		{
			border_color = _bordercolor;
			CacheRenderTarget rTargetCache(renderer);
			SDL_SetRenderTarget(renderer, this->texture.get());
			renderClear(renderer, 0, 0, 0, 0);
			if (border_sz > 0)
			{
				if (has_fading_border)
				{
					const uint8_t cache_border_alpha = border_color.a;
					const uint8_t alpha_step = floor(255 / border_sz);
					SDL_FRect border_rect = {(float)border_sz - 1.f, (float)border_sz - 1.f, dest.w + 2.f,
											 this->dest.h + 2.f};
					for (int i = 0; i < border_sz; ++i)
					{
						drawRoundedRectF(renderer, border_rect, rad_, border_color);
						border_rect.x -= 1.f;
						border_rect.y -= 1.f;
						border_rect.w += 2.f;
						border_rect.h += 2.f;
						border_color.a -= alpha_step;
					}
					border_color.a = cache_border_alpha;
				}
				else
				{
					fillRoundedRectF(renderer,
									 {0, 0, this->dest.w + (border_sz * 2.f),
									  this->dest.h + (border_sz * 2.f)},
									 rad_, border_color);
				}
			}
			/*fillRoundedRect(renderer,
							{0, 0, this->dest.w + (border_sz * 2), this->dest.h + (border_sz * 2)},
							rad_, border_color);*/
			fillRoundedRectF(renderer,
							 {(float)border_sz, (float)border_sz, this->dest.w, this->dest.h}, rad_,
							 inner_color);
			rTargetCache.release(renderer);
		}

		void Draw() const
		{
			if (inner_color.a < 1 && border_color.a < 1)
				return;
			UIWidget::Draw(renderer);
		}
	};

	class Circle : public UIWidget, public Context
	{
	public:
		Circle() {}

		Circle(Context *context_, const int &x_centre, const int &y_centre, const int &r,
			   const SDL_Color &col, const bool &filled = true,
			   const unsigned short &quadrant = 0)
		{
			Context::setContext(context_);
			this->Build(x_centre, y_centre, r, col, filled, quadrant);
		}

		Circle &setContext(Context *context_)
		{
			Context::setContext(context_);
			return *this;
		}

		void Build(const float &x_centre, const float &y_centre, const float &r,
				   const SDL_Color &col, const bool &filled = true,
				   const unsigned short &quadrant = 0)
		{
			if (quadrant == 0)
				dest = {r, r, r + r, r + r};
			else if (quadrant == 1)
				dest = {x_centre - r, y_centre - r, r, r};
			else if (quadrant == 2)
				dest = {x_centre, y_centre - r, r, r};
			else if (quadrant == 3)
				dest = {x_centre - r, y_centre, r, r};
			else if (quadrant == 4)
				dest = {x_centre, y_centre, r, r};

			if (this->texture.get() != nullptr)
			{
				SDL_DestroyTexture(this->texture.get());
				this->texture.reset();
			}

			this->texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
												SDL_TEXTUREACCESS_TARGET, this->dest.h + 1,
												this->dest.h + 1);
			SDL_SetTextureBlendMode(this->texture.get(), SDL_BLENDMODE_BLEND);
			CacheRenderTarget rTargetCache(renderer);
			SDL_SetRenderTarget(renderer, this->texture.get());
			renderClear(renderer, 0, 0, 0, 0);

			if (filled)
				FillCircle(renderer, dest.x + 1, dest.y + 1, r - 1, col),
					DrawCircle(renderer, dest.x, dest.y, r, border_color);

			else
				DrawCircle(renderer, dest.x, dest.y, r, col);
			dest.x = x_centre;
			dest.y = y_centre;

			rTargetCache.release(renderer);
		}

		void set_pos(int x, int y)
		{
			dest.x = x - dest.w / 2;
			dest.y = y - dest.w / 2;
		}
	};

	class Button
	{
	public:
		SDL_FRect dest = {0, 0, 0, 0};
		bool ISDOWN = false;

		Button()
		{
			/// TODO
		}

		void handleEvent(const SDL_Event &event)
		{
			switch (event.type)
			{
			case SDL_EVENT_FINGER_DOWN:
				if (testClick(event.tfinger.x, event.tfinger.y))
					ISDOWN = true;
				break;
			case SDL_EVENT_FINGER_UP:
				ISDOWN = false;
				break;
			}
		}

		bool testClick(float x, float y, short axis = 0)
		{
			if (axis == 0)
			{
				if (x > dest.x && x < (dest.w + dest.w))
					if (y > dest.y && y < (dest.h + dest.y))
						return true;
			}
			else if (axis == 1 /*x-axis only*/)
			{
				if (x > dest.x && x < (dest.w + dest.w))
					return true;
			}
			else if (axis == 2 /*y-axis only*/)
			{
				if (y > dest.y && y < (dest.h + dest.y))
					return true;
			}
			return false;
		}
	};

	struct Style
	{
		SDL_FRect rect;
		TextAttributes text;
		TextWrapStyle text_wrap;
		FontStyle font;
		std::string font_file;
		EdgeType edge;
		Gravity gravity;
		Margin margin;
		uint32_t maxlines = 1;
		bool shrink_to_fit = false;
		bool highlightOnHover = false;
	};

	struct ImageButtonAttributes
	{
		SDL_FRect rect = {0.f, 0.f, 0.f, 0.f};
		// inner image box dimensions in percentage {%x,%y,%w,%h} relative to rect
		SDL_FRect percentage_img_rect = {0.f, 0.f, 100.f, 100.f};
		std::string image_path;
		// image loading style
		IMAGE_LD_STYLE image_load_style = IMAGE_LD_STYLE::NORMAL;
		SDL_Texture *custom_texture = nullptr;
		std::function<SDL_Surface *()> getImageSurface = nullptr;
		uint32_t async_load_timeout = 0;
		float corner_radius = 0.f;
		SDL_Color bg_color = {0x00, 0x00, 0x00, 0x00};
		float shrink_size = 0.f;
	};

	class ImageButton : public Context, IView
	{
	public:
		// using IView::bounds;
		using IView::getView;

	public:
		ImageButton &setContext(Context *_context)
		{
			Context::setContext(_context);
			adaptiveVsyncHD.setAdaptiveVsync(adaptiveVsync);
			return *this;
		}

		ImageButton &setShrinkSize(const float &_shrink_size_in_percentage)
		{
			shrink_perc_ = std::clamp(_shrink_size_in_percentage, 0.f, 100.f);
			return *this;
		}

		/**
		 * Add a callback which will be invoked during a on mouse click event
		 * The callbacks are invoked in the order they were added/registered
		 */
		ImageButton &registerOnClickedCallBack(std::function<void()> _onClickedCallBack)
		{
			onClickedCallBack_ = _onClickedCallBack;
			return *this;
		}

		void setEnabled(bool is_enabled)
		{
			this->enabled = is_enabled;
		}

		ImageButton &Build(const ImageButtonAttributes &_imageButtonAttributes)
		{
			if (_imageButtonAttributes.image_load_style == IMAGE_LD_STYLE::NORMAL)
			{
				this->Build(_imageButtonAttributes.image_path, _imageButtonAttributes.rect, _imageButtonAttributes.percentage_img_rect, _imageButtonAttributes.corner_radius, _imageButtonAttributes.bg_color);
			}
			else if (_imageButtonAttributes.image_load_style == IMAGE_LD_STYLE::CUSTOM_TEXTURE)
			{
				this->BuildWithTexture(_imageButtonAttributes.custom_texture, _imageButtonAttributes.rect, _imageButtonAttributes.percentage_img_rect, _imageButtonAttributes.corner_radius, _imageButtonAttributes.bg_color);
			}
			else if (_imageButtonAttributes.image_load_style == IMAGE_LD_STYLE::ASYNC_PATH)
			{
				this->BuildAsync(_imageButtonAttributes.image_path, _imageButtonAttributes.rect, _imageButtonAttributes.percentage_img_rect, _imageButtonAttributes.corner_radius, _imageButtonAttributes.bg_color);
			}
			else if (_imageButtonAttributes.image_load_style == IMAGE_LD_STYLE::ASYNC_CUSTOM_SURFACE_LOADER)
			{
				this->BuildAsync(_imageButtonAttributes.getImageSurface, _imageButtonAttributes.rect, _imageButtonAttributes.percentage_img_rect, _imageButtonAttributes.corner_radius, _imageButtonAttributes.bg_color);
			}
			else if (_imageButtonAttributes.image_load_style == IMAGE_LD_STYLE::ASYNC_DEFAULT_TEXTURE_PATH)
			{
			}
			else if (_imageButtonAttributes.image_load_style == IMAGE_LD_STYLE::ASYNC_DEFAULT_TEXTURE_CUSTOM_LOADER)
			{
				this->BuildAsyncWithDefaultTexture(_imageButtonAttributes.getImageSurface, _imageButtonAttributes.custom_texture, _imageButtonAttributes.rect, _imageButtonAttributes.percentage_img_rect, _imageButtonAttributes.corner_radius, _imageButtonAttributes.bg_color);
			}
			this->setShrinkSize(_imageButtonAttributes.shrink_size);
			this->configureShrinkSize();
			return *this;
		}

		ImageButton &Build(const std::string &_img_path, const SDL_FRect &_rect, const SDL_FRect &_percentage_img_rect = {0.f, 0.f, 100.f, 100.f},
						   const float &_corner_radius = 0.f, const SDL_Color &_bg_color = {0x00, 0x00, 0x00, 0x00})
		{
			SDL_Texture *tmp_texture_ = IMG_LoadTexture(renderer, _img_path.c_str());
			if (not tmp_texture_)
			{
				SDL_Log("couldn't load img: %s : %s", _img_path.c_str(), SDL_GetError());
			}
			build_comon(tmp_texture_, _rect, _percentage_img_rect, _corner_radius, _bg_color);
			DestroyTextureSafe(tmp_texture_);
			return *this;
		}

		ImageButton &BuildWithTexture(SDL_Texture *_custom_texture, const SDL_FRect &_rect, const SDL_FRect &_percentage_img_rect = {0.f, 0.f, 100.f, 100.f},
									  const float &_corner_radius = 0.f, const SDL_Color &_bg_color = {0x00, 0x00, 0x00, 0x00})
		{
			build_comon(_custom_texture, _rect, _percentage_img_rect, _corner_radius, _bg_color);
			return *this;
		}

		ImageButton &BuildAsync(const std::string &_img_path, const SDL_FRect &_rect, const SDL_FRect &_percentage_img_rect = {0.f, 0.f, 100.f, 100.f},
								const float &_corner_radius = 0.f, const SDL_Color &_bg_color = {0x00, 0x00, 0x00, 0x00})
		{
			build_with_async_ = true;
			build_comon(nullptr, _rect, _percentage_img_rect, _corner_radius, _bg_color);
			// async_then = std::chrono::high_resolution_clock::now();
			// async_load_future_ = std::async(std::launch::async, &ImageButton::async_img_Load, this, _img_path);
			async_load_future_ = executor_.enqueue(&ImageButton::async_img_Load, this, _img_path);
			adaptiveVsyncHD.startRedrawSession();
			return *this;
		}

		ImageButton &BuildAsync(std::function<SDL_Surface *()> _customSurfaceLoader, const SDL_FRect &_rect,
								const SDL_FRect &_percentage_img_rect = {0.f, 0.f, 100.f, 100.f}, const float &_corner_radius = 0.f,
								const SDL_Color &_bg_color = {0x00, 0x00, 0x00, 0x00})
		{
			build_with_async_ = true;
			build_comon(nullptr, _rect, _percentage_img_rect, _corner_radius, _bg_color);
			// async_then = std::chrono::high_resolution_clock::now();
			// async_load_future_ = std::async(std::launch::async, _customSurfaceLoader);
			async_load_future_ = executor_.enqueue(_customSurfaceLoader);
			adaptiveVsyncHD.startRedrawSession();
			return *this;
		}

		ImageButton &BuildAsyncWithDefaultTexture(const std::string &_img_path, SDL_Texture *_custom_texture, const SDL_FRect &_rect,
												  const SDL_FRect &_percentage_img_rect = {0.f, 0.f, 100.f, 100.f}, const float &_corner_radius = 0.f,
												  const SDL_Color &_bg_color = {0x00, 0x00, 0x00, 0x00})
		{
			build_with_async_ = true;
			build_comon(_custom_texture, _rect, _percentage_img_rect, _corner_radius, _bg_color);
			// async_then = std::chrono::high_resolution_clock::now();
			async_load_future_ = std::async(std::launch::async, &ImageButton::async_img_Load, this, _img_path);
			adaptiveVsyncHD.startRedrawSession();
			return *this;
		}

		ImageButton &
		BuildAsyncWithDefaultTexture(std::function<SDL_Surface *()> _customSurfaceLoader, SDL_Texture *_custom_texture, const SDL_FRect &_rect,
									 const SDL_FRect &_percentage_img_rect = {0.f, 0.f, 100.f, 100.f}, const float &_corner_radius = 0.f,
									 const SDL_Color &_bg_color = {0x00, 0x00, 0x00, 0x00}) noexcept
		{
			build_with_async_ = true;
			build_comon(_custom_texture, _rect, _percentage_img_rect, _corner_radius, _bg_color);
			// async_then = std::chrono::high_resolution_clock::now();
			async_load_future_ = std::async(std::launch::async, _customSurfaceLoader);
			adaptiveVsyncHD.startRedrawSession();
			return *this;
		}

		void Draw() override
		{
			[[unlikely]] if (build_with_async_)
				checkAndProcessAsyncProgress();
			SDL_RenderTexture(renderer, texture_.get(), nullptr, &bounds);
		}

		SDL_FRect &getRect() noexcept
		{
			return getBoundsBox();
		}

		bool isAsyncLoadComplete() const noexcept
		{
			return is_async_load_done();
			// return !build_with_async_;
		}

		bool handleEvent() override
		{
			if (!enabled)
				return false;
			// SDL_Log("HECKNYEAH");
			bool result = false;
			if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN)
			{
				if (isPointInBound(event->button.x, event->button.y))
				{
					// if(type=="submit")
					touch_down_ = true;
					shrinkButton();
					result = true;
					// SDL_Log("touch down");
				}
			}
			else if (event->type == SDL_EVENT_FINGER_DOWN)
			{
				if (isPointInBound(event->tfinger.x * DisplayInfo::Get().RenderW, event->tfinger.y * DisplayInfo::Get().RenderH))
				{
					touch_down_ = true;
					result = true;
					shrinkButton();
					// SDL_Log("touch down");
				}
			}
			else if (event->type == SDL_EVENT_MOUSE_MOTION)
			{
				motion_occured_ = true;
			}
			else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP)
			{
				unshrinkButton();
				if (isPointInBound(event->button.x, event->button.y) && touch_down_ /* && !motion_occured_*/)
				{
					// SDL_Log("released");
					if (onClickedCallBack_ != nullptr)
						onClickedCallBack_();
					result = true;
				}
				touch_down_ = false;
				motion_occured_ = false;
			}
			else if (event->type == SDL_EVENT_FINGER_UP)
			{
				unshrinkButton();
				if (isPointInBound(event->tfinger.x * DisplayInfo::Get().RenderW, event->tfinger.y * DisplayInfo::Get().RenderH) && touch_down_ /* && !motion_occured_*/)
				{
					// SDL_Log("released");
					if (onClickedCallBack_ != nullptr)
						onClickedCallBack_();
					result = true;
				}
				touch_down_ = false;
				motion_occured_ = false;
			}
			else if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
			{
				auto cw = bounds.w;
				auto ch = bounds.h;
				bounds =
					{
						DisplayInfo::Get().toUpdatedWidth(bounds.x),
						DisplayInfo::Get().toUpdatedHeight(bounds.y),
						DisplayInfo::Get().toUpdatedWidth(bounds.w),
						DisplayInfo::Get().toUpdatedHeight(bounds.h),
					};
				adjust_rect_to_fit(cw, ch);
			}

			return result;
		}

		void updatePosBy(float _dx, float _dy)
		{
			IView::updatePosBy(_dx, _dy);
			// return *this;
		}

		SDL_Texture *getTexture() const
		{
			return texture_.get();
		}

	private:
		void build_comon(SDL_Texture *_texture, const SDL_FRect &_rect, const SDL_FRect &_percentage_img_rect, const float &_corner_radius,
						 const SDL_Color &_bg_color)
		{
			bounds = _rect;
			img_rect_.x = to_cust(_percentage_img_rect.x, _rect.w);
			img_rect_.y = to_cust(_percentage_img_rect.y, _rect.h);
			img_rect_.w = to_cust(_percentage_img_rect.w, _rect.w);
			img_rect_.h = to_cust(_percentage_img_rect.h, _rect.h);

			if (!build_with_async_)
			{
				int rw, rh;
				SDL_QueryTexture(_texture, nullptr, nullptr, &rw, &rh);
				// adjust_image_rect(rw, rh);
			}

			corner_radius_ = _corner_radius;
			bg_color_ = _bg_color;

			CacheRenderTarget cache_r_target(renderer);
			texture_.reset();
			texture_ = CreateSharedTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, (int)(bounds.w),
										   (int)(bounds.h));
			SDL_SetTextureBlendMode(texture_.get(), SDL_BLENDMODE_BLEND);
			SDL_SetRenderTarget(renderer, texture_.get());
			renderClear(renderer, 0, 0, 0, 0);
			fillRoundedRectF(renderer, {0.f, 0.f, bounds.w, bounds.h}, corner_radius_, bg_color_);
			// renderClear(renderer, _bg_color.r, _bg_color.g, _bg_color.b, _bg_color.a);
			SDL_RenderTexture(renderer, _texture, nullptr, &img_rect_);
			cache_r_target.release(renderer);
			configureShrinkSize();
		}

		bool is_async_load_done() const noexcept
		{
			// potential double check for instances created with async
			// one in the draw call && the other on check_and_process_async
			if (!build_with_async_)
				return true;
			if (/*async_load_future_._Is_ready()*/ async_load_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
				return true;
			return false;
		}

		void checkAndProcessAsyncProgress() noexcept
		{
			if (is_async_load_done())
			{
				adaptiveVsyncHD.stopRedrawSession();
				SDL_Surface *async_res_ = async_load_future_.get();
				if (!async_res_)
				{
					SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Async Image returned null");
					build_with_async_ = false;
					return;
				}
				SDL_Texture *async_res_tex_ = SDL_CreateTextureFromSurface(renderer, async_res_);
				const int rw = async_res_->w, rh = async_res_->h;
				// adjust_image_rect(rw, rh);
				adjust_image_rect_to_fit(rw, rh);
				async_free1_ = executor_.enqueue(SDL_DestroySurface, async_res_);
				CacheRenderTarget crt_(renderer);
				SDL_SetRenderTarget(renderer, texture_.get());
				// SDL_SetRenderDrawColor(renderer, bg_color_.r, bg_color_.g, bg_color_.b, bg_color_.a);
				// SDL_SetRenderDrawColor(renderer, 0xff, 0, 0, 0xff);
				// SDL_RenderFillRect(renderer, &img_rect_);
				renderClear(renderer, bg_color_.r, bg_color_.g, bg_color_.b, bg_color_.a);
				SDL_RenderTexture(renderer, async_res_tex_, nullptr, &img_rect_);
				crt_.release(renderer);
				SDL_DestroyTexture(async_res_tex_);
				// async_free2_ = executor_.enqueue(DestroyTextureSafe, async_res_tex_);
				// async_free2_ = std::async(std::launch::async, SDL_DestroyTexture, async_res_tex_);
				build_with_async_ = false;
				// std::chrono::duration<double> dt = (std::chrono::high_resolution_clock::now() - async_then);
				// SDL_Log("ASYNC_LOAD_TM: %f", dt.count());
			}
		}

		const bool isPointInBound(float x, float y) const noexcept
		{
			/*const SDL_FPoint tp_ = { x,y };
			return SDL_PointInRectFloat(&tp_, &rect_);*/
			// SDL_Log("pvx: %f, pvy: %f, pvw: %f", pv->rect.x, pv->rect.y, pv->rect.w);
			if (x > pv->bounds.x + bounds.x && x < (pv->bounds.x + bounds.x + bounds.w) && y > pv->bounds.y + bounds.y && y < pv->bounds.y + bounds.y + bounds.h)
				return true;

			return false;
		}

		void adjust_image_rect_to_fit(int width, int height)
		{
			int final_width, final_height;
			int max_width = img_rect_.w, max_height = img_rect_.h;
			float aspect_ratio = (float)width / (float)height;
			if (width <= max_width && height <= max_height)
			{
				final_width = width;
				final_height = height;
			}
			else if (width > max_width && height > max_height)
			{
				if (aspect_ratio >= 1)
				{
					final_width = max_width;
					final_height = (int)(max_width / aspect_ratio);
				}
				else
				{
					final_height = max_height;
					final_width = (int)(max_height * aspect_ratio);
				}
			}
			else if (width > max_width)
			{
				final_width = max_width;
				final_height = (int)(max_width / aspect_ratio);
			}
			else
			{
				final_height = max_height;
				final_width = (int)(max_height * aspect_ratio);
			}

			img_rect_.x += ((img_rect_.w - final_width) / 2.f);
			img_rect_.w = final_width;
			img_rect_.y += ((img_rect_.h - final_height) / 2.f);
			img_rect_.h = final_height;
		}

		void adjust_rect_to_fit(int width, int height)
		{
			int final_width, final_height;
			int max_width = bounds.w, max_height = bounds.h;
			float aspect_ratio = (float)width / (float)height;
			if (width <= max_width && height <= max_height)
			{
				final_width = width;
				final_height = height;
			}
			else if (width > max_width && height > max_height)
			{
				if (aspect_ratio >= 1)
				{
					final_width = max_width;
					final_height = (int)(max_width / aspect_ratio);
				}
				else
				{
					final_height = max_height;
					final_width = (int)(max_height * aspect_ratio);
				}
			}
			else if (width > max_width)
			{
				final_width = max_width;
				final_height = (int)(max_width / aspect_ratio);
			}
			else
			{
				final_height = max_height;
				final_width = (int)(max_height * aspect_ratio);
			}

			bounds.x += ((bounds.w - final_width) / 2.f);
			bounds.w = final_width;
			bounds.y += ((bounds.h - final_height) / 2.f);
			bounds.h = final_height;
		}

		void shrinkButton() noexcept
		{
			if (shrinked_)
				return;
			// SDL_Log("Shrinking %f", shrink_size_);
			// SDL_Log("BF_Shrinking %f", rect_.w);
			bounds.x += shrink_size_;
			bounds.y += shrink_size_;
			bounds.w -= shrink_size_ * 2.f;
			bounds.h -= shrink_size_ * 2.f;
			// SDL_Log("shrink");
			// SDL_Log("AF_Shrinking %f", rect_.w);
			shrinked_ = true;
		}

		void unshrinkButton() noexcept
		{
			if (!shrinked_)
				return;
			bounds.x -= shrink_size_;
			bounds.y -= shrink_size_;
			bounds.w += shrink_size_ * 2.f;
			bounds.h += shrink_size_ * 2.f;
			shrinked_ = false;
			// SDL_Log("un shrink");
		}

		void configureShrinkSize() noexcept
		{
			if (bounds.w > bounds.h)
			{
				shrink_size_ = to_cust(shrink_perc_, bounds.h);
			}
			else if (bounds.w <= bounds.h)
			{
				shrink_size_ = to_cust(shrink_perc_, bounds.w);
			}
		}

		SDL_Surface *async_img_Load(const std::string &_path) noexcept
		{
			SDL_Surface *surface;
			if (!(surface = IMG_Load(_path.c_str())))
				SDL_Log("%s", IMG_GetError());
			// SDL_Delay(10000);
			return surface;
		}

	private:
		uint32_t async_start;
		SharedTexture texture_;
		SDL_FRect img_rect_;
		// SDL_FRect rect_;
		float corner_radius_ = 0.f;
		float shrink_size_ = 0.f, shrink_perc_ = 10.f;
		SDL_Color bg_color_;
		bool touch_down_ = false, motion_occured_ = false, shrinked_ = false, build_with_async_ = false;
		bool enabled = true;
		std::function<void()> onClickedCallBack_ = nullptr;
		std::shared_future<SDL_Surface *> async_load_future_;
		std::shared_future<void> async_free1_;
		std::shared_future<void> async_free2_;
		static Async::ThreadPool executor_;
		AdaptiveVsyncHandler adaptiveVsyncHD;
	};

	Async::ThreadPool ImageButton::executor_(1);

	class Cursor : public Context
	{
	public:
		uint32_t m_start;
		int vpos, hpos, pos;

		Cursor()
		{
			m_blink_tm = 1;
			m_color = {0x0, 0x0, 0x0, 0xff};
			vpos = hpos = pos = 0;
		}

		Cursor(const SDL_FRect &rect, const SDL_Color &color, const uint32_t &blink_time,
			   const int &step_size) : m_rect(rect), m_color(color), m_blink_tm(blink_time), vpos(0), hpos(0) {}

		Cursor &setContext(Context *context_)
		{
			Context::setContext(context_);
			return *this;
		}

		Cursor &configure(const SDL_FRect &rect, const SDL_Color &color, const MilliSec<uint32_t> &blink_time)
		{
			this->m_rect = rect;
			this->m_color = color;
			this->m_blink_tm = blink_time.get();
			return *this;
		}

		Cursor &setRect(const SDL_FRect &rect)
		{
			this->m_rect = rect;
			return *this;
		}

		SDL_FRect getRect()
		{
			return this->m_rect;
		}

		Cursor &setPosX(const float &posX)
		{
			this->m_rect.x = posX;
			return *this;
		}

		Cursor &setColor(const SDL_Color &color)
		{
			this->m_color = color;
			return *this;
		}

		Cursor &setBlinkTm(const uint32_t &blink_time)
		{
			this->m_blink_tm = blink_time;
			return *this;
		}

		void Draw()
		{
			uint32_t diff_ = SDL_GetTicks() - m_start;
			if (diff_ <= m_blink_tm)
				CacheRenderColor(renderer),
					SDL_SetRenderDrawColor(renderer, m_color.r, m_color.g,
										   m_color.b,
										   m_color.a),
					SDL_RenderFillRect(renderer, &m_rect),
					RestoreCachedRenderColor(renderer);
			else if (diff_ >= m_blink_tm * 2)
				m_start = SDL_GetTicks();
		}

	private:
		SDL_FRect m_rect;
		uint32_t m_blink_tm;
		SDL_Color m_color;
	};

	struct U8TextBoxConfigData
	{
		SDL_FRect rect;
		U8TextAttributes text_attributes = {u8"", {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff}};
		SDL_FRect margin = {0.f, 0.f, 100.f, 100.f};
		std::string font_file;
		FontStyle font_style;
		EdgeType edge_type;
		uint32_t maxlines = 1;
		TextWrapStyle text_wrap_style = TextWrapStyle::MAX_CHARS_PER_LN;
		Gravity gravity = Gravity::LEFT;
		float coner_radius = 0.f;
		int custom_fontstyle = 0x00;
		bool shrink_to_fit = false;
	};

	struct TextBoxAttributes
	{
		Font mem_font = Font::ConsolasBold;
		SDL_FRect rect = {0.f, 0.f, 0.f, 0.f};
		TextAttributes textAttributes = {"", {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff}};
		SDL_FRect margin = {0.f, 0.f, 100.f, 100.f};
		std::string fontFile;
		FontStyle fontStyle = FontStyle::NORMAL;
		EdgeType edgeType = EdgeType::RECT;
		uint32_t maxlines = 1;
		TextWrapStyle textWrapStyle = TextWrapStyle::MAX_CHARS_PER_LN;
		Gravity gravity = Gravity::LEFT;
		float conerRadius = 0.f;
		int customFontstyle = 0x00;
		float lineSpacing = 0.f;
		float outline = 0.f;
		bool isButton = false;
		bool shrinkToFit = false;
		bool highlightOnHover = false;
		SDL_Color outlineColor = {0x00, 0x00, 0x00, 0x00};
		SDL_Color onHoverOutlineColor = {0x00, 0x00, 0x00, 0x00};
		SDL_Color onHoverBgColor = {0x00, 0x00, 0x00, 0x00};
		SDL_Color onHoverTxtColor = {0x00, 0x00, 0x00, 0x00};
	};

	class TextBoxBuilder;

	class TextBox : public Context, IView
	{
	public:
		using IView::getView;
		using IView::isHidden;
		using IView::type;

	public:
		friend class TextBoxBuilder;
		TextBox &setContext(Context *_context)
		{
			Context::setContext(_context);
			Context::setView(this);
			return *this;
		}

		static TextBoxBuilder Builder(Context *context);

		TextBox &Build(Context *_context, const TextBoxAttributes &textboxAttr_)
		{
			Context::setContext(_context);
			config_dat_ = textboxAttr_;
			coner_radius_ = textboxAttr_.conerRadius;
			line_skip_ = textboxAttr_.lineSpacing;
			isButton = textboxAttr_.isButton;
			bounds = textboxAttr_.rect;
			outlineRect.Build(this, textboxAttr_.rect, textboxAttr_.outline, textboxAttr_.conerRadius, textboxAttr_.textAttributes.bg_color, textboxAttr_.outlineColor);

			text_rect_ = {to_cust(textboxAttr_.margin.x, bounds.w),
						  to_cust(textboxAttr_.margin.y, bounds.h),
						  to_cust(textboxAttr_.margin.w - textboxAttr_.margin.x, bounds.w),
						  to_cust(textboxAttr_.margin.h, bounds.h)};
			text_attributes_ = textboxAttr_.textAttributes;
			font_attributes_.font_file = textboxAttr_.fontFile;
			font_attributes_.font_style = textboxAttr_.fontStyle;
			// font_attributes_.font_size = static_cast<uint8_t>(to_cust(95.f, text_rect_.h));
			font_attributes_.font_size = static_cast<uint8_t>(text_rect_.h);
			custom_fontstyle_ = textboxAttr_.customFontstyle;
			max_lines_ = textboxAttr_.maxlines;
			edge_type_ = textboxAttr_.edgeType;
			text_wrap_style_ = textboxAttr_.textWrapStyle;
			gravity_ = textboxAttr_.gravity;
			shrink_to_fit = textboxAttr_.shrinkToFit;
			wrapped_text_.clear();

			max_displayable_chars_per_ln_ = static_cast<uint32_t>(text_rect_.w / (text_rect_.h / 2.f));
			if (text_attributes_.text.size() < max_displayable_chars_per_ln_)
				wrapped_text_.emplace_back(text_attributes_.text);
			else
			{
				if (text_wrap_style_ == TextWrapStyle::MAX_WORDS_PER_LN)
					TextProcessor::Get().wrap_max_word_count(text_attributes_.text, &wrapped_text_, " ,-_./\:;|})!",
															 max_displayable_chars_per_ln_, max_lines_);
				else
					TextProcessor::Get().wrap_max_char(text_attributes_.text, &wrapped_text_, max_displayable_chars_per_ln_, max_lines_);
			}
			/*if(max_displayable_chars_per_ln_<text_attributes_.text.size()){
				wrapped_text_.back() = wrapped_text_.back().replace(wrapped_text_.back().begin()+ (wrapped_text_.back().size()-3), wrapped_text_.back().end(),"...");
			}*/
			max_displayable_lines_ = std::clamp(static_cast<uint32_t>(std::floorf(bounds.h / text_rect_.h)), (uint32_t)1, (uint32_t)1000000);
			capture_src_ = {0.f, 0.f, text_rect_.w, text_rect_.h * std::clamp((float)wrapped_text_.size(), 1.f, (float)max_displayable_lines_)};
			dest_src_ = text_rect_;
			dest_src_.h = capture_src_.h;
			dest_src_.x += bounds.x;
			dest_src_.y += bounds.y;

			this->texture_.reset();
			// CacheRenderTarget crt_(renderer);
			this->texture_ = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
												 SDL_TEXTUREACCESS_TARGET, static_cast<int>(text_rect_.w),
												 static_cast<int>(text_rect_.h * wrapped_text_.size()));
			// SDL_SetTextureBlendMode(this->texture_.get(), SDL_BLENDMODE_BLEND);
			// SDL_SetRenderTarget(renderer, this->texture_.get());
			// renderClear(renderer, 0, 0, 0, 0);
			// FontSystem::Get().setFontAttributes({ font_attributes_.font_file.c_str(), font_attributes_.font_style, font_attributes_.font_size, 0.f }, custom_fontstyle_);

			genText();
			return *this;
		}

	public:
		void Draw() override
		{
			/*SDL_SetRenderDrawColor(renderer, text_attributes_.bg_color.r, text_attributes_.bg_color.g, text_attributes_.bg_color.b, text_attributes_.bg_color.a);
			SDL_RenderFillRect(renderer, &dest_);*/
			outlineRect.Draw();
			// fillRoundedRectF(renderer, dest_, coner_radius_, text_attributes_.bg_color);
			SDL_RenderTexture(renderer, texture_.get(), &capture_src_, &dest_src_);
			// return *this;
		}

		TextBox &addOnclickedCallback(std::function<void(TextBox *)> _on_clicked_callback) noexcept
		{
			onClickedCallback_ = _on_clicked_callback;
			return *this;
		}

		constexpr inline TextBox &setConerRadius(const float &cr_) noexcept
		{
			this->coner_radius_ = cr_;
			return *this;
		}

		TextBox &setId(const uint32_t &_id) noexcept
		{
			this->id_ = _id;
			return *this;
		}

		SDL_FRect &getBounds()
		{
			return bounds;
		}

		inline const SDL_FRect &getBoundsConst() const noexcept
		{
			return this->bounds;
		}

		TextBox &setPos(const float x, const float y)
		{

			return *this;
		}

		inline void updatePosBy(float x, float y) override
		{
			update_pos_internal(x, y, false);
			// return *this;
		}

		TextBox &updatePosByAnimated(const float x, const float y)
		{
			update_pos_internal(x, y, true);
			return *this;
		}

		std::string getText() const
		{
			return this->text_attributes_.text;
		}

		bool isEnabled() const
		{
			return is_enabled_;
		}

		void setEnabled(const bool &_enabled)
		{
			is_enabled_ = _enabled;
		}

		uint32_t getId() const
		{
			return this->id_;
		}

		TextBox &updateTextColor(const SDL_Color &bgColor, const SDL_Color &outlineColor, const SDL_Color &textColor)
		{
			text_attributes_.bg_color = bgColor;
			text_attributes_.text_color = textColor;
			outlineRect.color = bgColor;
			outlineRect.outline_color = outlineColor;

			CacheRenderTarget crt_(renderer);
			SDL_SetRenderTarget(renderer, this->texture_.get());
			SDL_SetTextureBlendMode(this->texture_.get(), SDL_BLENDMODE_BLEND);
			renderClear(renderer, 0, 0, 0, 0);
			const SDL_FRect cache_text_rect = text_rect_;
			int tmp_sw = 0, tmp_sh = 0;
			text_rect_.y = 0.f;
			text_rect_.x = 0.f;

			TTF_Font *tmpFont = nullptr;
			if (font_attributes_.font_file.empty())
			{
				Fonts[config_dat_.mem_font]->font_size = font_attributes_.font_size;
				font_attributes_.font_file = Fonts[config_dat_.mem_font]->font_name;
				tmpFont = FontSystem::Get().getFont(*Fonts[config_dat_.mem_font]);
			}
			else
			{
				tmpFont = FontSystem::Get().getFont(font_attributes_.font_file, font_attributes_.font_size);
			}
			const float fa_ = static_cast<float>(TTF_FontDescent(tmpFont));
			FontSystem::Get().setFontAttributes({font_attributes_.font_file.c_str(), font_attributes_.font_style, font_attributes_.font_size}, custom_fontstyle_);
			for (auto const &line_ : wrapped_text_)
			{
				auto textTex = FontSystem::Get().genTextTextureUnique(renderer, line_.c_str(), this->text_attributes_.text_color);
				if (textTex.has_value())
				{
					SDL_QueryTexture(textTex.value().get(), nullptr, nullptr, &tmp_sw, &tmp_sh);
					// SDL_Log("TW: %d, TH: %d", tmp_sw, tmp_sh);
					text_rect_.w = static_cast<float>(tmp_sw);
					// text_rect_.h = (float)(tmp_sh);
					text_rect_.w = std::clamp(text_rect_.w, 0.f, cache_text_rect.w);

					if (max_lines_ == 1)
					{
						if (gravity_ == Gravity::CENTER)
						{
							text_rect_.x = ((cache_text_rect.w - text_rect_.w) / 2.f);
						}
						if (shrink_to_fit)
						{
							float final_rad = 1.f;
							if (bounds.h > bounds.w)
							{
								final_rad = ((coner_radius_ / 2.f) * bounds.w) / 100.f;
							}
							else if (bounds.h <= bounds.w)
							{
								final_rad = ((coner_radius_ / 2.f) * bounds.h) / 100.f;
							}
							// dest_.x = dest_.x + (text_rect_.x - final_rad);
							// dest_.w = text_rect_.w + (final_rad * 2.f);
						}
					}
					SDL_RenderTexture(renderer, textTex.value().get(), nullptr, &text_rect_);
					text_rect_.y += line_skip_ + text_rect_.h;
				}
				else
					break;
			}
			text_rect_ = cache_text_rect;
			crt_.release(renderer);
			return *this;
		}

		void resolveTextureReset()
		{
			// this->Build(this, config_dat_);
			updateTextColor(config_dat_.textAttributes.bg_color, config_dat_.outlineColor, config_dat_.textAttributes.text_color);
		}

		bool handleEvent()
		{
			// SDL_Log("tb handle event");
			bool result = false;
			switch (event->type)
			{
			case SDL_EVENT_RENDER_TARGETS_RESET:
				resolveTextureReset();
				result = true;
				break;
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				if (onClick(event->button.x, event->button.y))
					mouse_in_bound_ = true, result=true;
				else
					mouse_in_bound_ = false;
				break;
			case SDL_EVENT_MOUSE_MOTION:
				if (onClick(event->motion.x, event->motion.y))
				{
					mouse_in_bound_ = true;
					if (config_dat_.highlightOnHover)
					{
						outlineRect.outline_color = config_dat_.onHoverOutlineColor;
						outlineRect.color = config_dat_.onHoverBgColor;
					}
				}
				else
				{
					mouse_in_bound_ = false;
					if (config_dat_.highlightOnHover)
					{
						outlineRect.outline_color = config_dat_.outlineColor;
						outlineRect.color = text_attributes_.bg_color;
					}
				}
				break; /*
			 case SDL_EVENT_MOUSE_BUTTON_UP:
				 if (onClick(event->motion.x, event->motion.y)) {
					 if (isButton)result = true;
					 if (onClickedCallback_!=nullptr) {
						 result = true;
						 onClickedCallback_(this);
					 }
				 }
				 mouse_in_bound_ = false;
				 break;*/
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			case SDL_EVENT_WINDOW_MAXIMIZED:
				config_dat_.rect =
					{
						DisplayInfo::Get().toUpdatedWidth(config_dat_.rect.x),
						DisplayInfo::Get().toUpdatedHeight(config_dat_.rect.y),
						DisplayInfo::Get().toUpdatedWidth(config_dat_.rect.w),
						DisplayInfo::Get().toUpdatedHeight(config_dat_.rect.h),
					};
				this->Build(this, config_dat_);
				break;
			case SDL_EVENT_FINGER_UP:
				if (onClick(event->tfinger.x * DisplayInfo::Get().RenderW, event->tfinger.y * DisplayInfo::Get().RenderH))
				{
					if (isButton)
						result = true;
					if (onClickedCallback_)
					{
						result = true;
						onClickedCallback_(this);
					}
				}
				mouse_in_bound_ = false;
				break;
			}
			return result;
		}

	private:
		void genText()
		{
			CacheRenderTarget crt_(renderer);
			SDL_SetTextureBlendMode(this->texture_.get(), SDL_BLENDMODE_BLEND);
			SDL_SetRenderTarget(renderer, this->texture_.get());
			renderClear(renderer, 0, 0, 0, 0);
			const SDL_FRect cache_text_rect = text_rect_;
			int tmp_sw = 0, tmp_sh = 0;
			text_rect_.y = 0.f;
			text_rect_.x = 0.f;
			TTF_Font *tmpFont = nullptr;

			if (font_attributes_.font_file.empty())
			{
				Fonts[config_dat_.mem_font]->font_size = font_attributes_.font_size;
				font_attributes_.font_file = Fonts[config_dat_.mem_font]->font_name;
				tmpFont = FontSystem::Get().getFont(*Fonts[config_dat_.mem_font]);
			}
			else
			{
				tmpFont = FontSystem::Get().getFont(font_attributes_.font_file, font_attributes_.font_size);
			}

			const auto fa_ = static_cast<float>(TTF_FontAscent(tmpFont));
			const auto fd_ = static_cast<float>(TTF_FontDescent(tmpFont));

			FontSystem::Get().setFontAttributes(std::move(FontAttributes{font_attributes_.font_file.c_str(), font_attributes_.font_style, font_attributes_.font_size}), custom_fontstyle_);
			for (auto const &line_ : wrapped_text_)
			{
				auto textTex = FontSystem::Get().genTextTextureUnique(renderer, line_.c_str(), this->text_attributes_.text_color);
				if (textTex.has_value())
				{
					SDL_QueryTexture(textTex.value().get(), nullptr, nullptr, &tmp_sw, &tmp_sh);
					// SDL_Log("TW: %d, TH: %d", tmp_sw, tmp_sh);
					text_rect_.w = static_cast<float>(tmp_sw);
					// std::cout << "TT: " <<tmp_sh<< " - FA: " << fa_ << " - FD: " << fd_ << std::endl;
					// text_rect_.h = static_cast<float>(tmp_sh);
					// text_rect_.h = std::clamp(text_rect_.h, 0.f, text_rect_.h);
					// text_rect_.w = std::clamp(text_rect_.w, 0.f, cache_text_rect.w);
					if (static_cast<float>(tmp_sh) > text_rect_.h)
						text_rect_.y += fd_, text_rect_.h = static_cast<float>(tmp_sh);

					if (max_lines_ == 1)
					{
						if (gravity_ == Gravity::CENTER)
						{
							text_rect_.x = ((cache_text_rect.w - text_rect_.w) / 2.f);
						}
						if (shrink_to_fit)
						{
							float final_rad = 1.f;
							if (bounds.h > bounds.w)
							{
								final_rad = ((coner_radius_ / 2.f) * bounds.w) / 100.f;
							}
							else if (bounds.h <= bounds.w)
							{
								final_rad = ((coner_radius_ / 2.f) * bounds.h) / 100.f;
							}
							bounds.x = bounds.x + (text_rect_.x - final_rad);
							bounds.w = text_rect_.w + (final_rad * 2.f);
							outlineRect.rect.x = outlineRect.rect.x + (text_rect_.x - final_rad);
							outlineRect.rect.w = text_rect_.w + (final_rad * 2.f);
						}
					}
					SDL_RenderTexture(renderer, textTex.value().get(), nullptr, &text_rect_);
					text_rect_.y += line_skip_ + text_rect_.h;
					text_rect_.y += fd_;
				}
				else
					break;
			}
			text_rect_ = cache_text_rect;
			if (max_lines_ > 1)
			{
				if (shrink_to_fit)
				{
					float final_rad = 1.f;
					if (bounds.h > bounds.w)
					{
						final_rad = ((coner_radius_ / 2.f) * bounds.w) / 100.f;
					}
					else if (bounds.h <= bounds.w)
					{
						final_rad = ((coner_radius_ / 2.f) * bounds.h) / 100.f;
					}
					bounds.x = bounds.x + (text_rect_.x - final_rad);
					bounds.w = text_rect_.w + (final_rad * 2.f);
					outlineRect.rect.x = outlineRect.rect.x + (text_rect_.x - final_rad);
					outlineRect.rect.w = text_rect_.w + (final_rad * 2.f);
				}
			}
			crt_.release(renderer);
		}

	protected:
		SDL_FRect text_rect_, dest_src_, capture_src_;
		RectOutline outlineRect;
		SharedTexture texture_;
		ImageButton image_button_;
		std::deque<std::string> wrapped_text_;
		std::function<void(TextBox *)> onClickedCallback_;
		EdgeType edge_type_;
		Gravity gravity_;
		TextWrapStyle text_wrap_style_;
		TextAttributes text_attributes_;
		FontAttributes font_attributes_;
		TextBoxAttributes config_dat_;
		bool isButton = false, is_enabled_, highlight_on_mouse_hover_, mouse_in_bound_, shrink_to_fit;
		float margin_ = 0.f, line_skip_ = 0.f, coner_radius_ = 0.f;
		uint32_t id_, max_displayable_chars_per_ln_, max_displayable_lines_ = 1, max_lines_ = 1;
		int custom_fontstyle_ = TTF_STYLE_NORMAL;

		template <typename T>
		bool onClick(T x, T y, unsigned short axis = 0)
		{
			[[likely]] if (axis == 0)
			{
				if (x < pv->bounds.x + bounds.x || x > (pv->bounds.x + bounds.x + bounds.w) || y < pv->bounds.y + bounds.y ||
					y > (pv->bounds.y + bounds.y + bounds.h))
					return false;
			}
			else if (axis == 1 /*x-axis only*/)
			{
				if (x < bounds.x || x > (bounds.x + bounds.w))
					return false;
			}
			else if (axis == 2 /*y-axis only*/)
			{
				if (y < bounds.y || y > (bounds.y + bounds.h))
					return false;
			}
			return true;
		}

		inline void update_pos_internal(const float &x, const float &y, const bool &_is_animated) noexcept
		{
			bounds.x += x, bounds.y += y;
			dest_src_.x += x, dest_src_.y += y;
			config_dat_.rect.x += x, config_dat_.rect.y += y;
			outlineRect.rect.x += x, outlineRect.rect.y += y;
		}
	};

	class TextBoxBuilder
	{
	public:
		TextBoxBuilder(Context *context)
		{
			context_ = context;
		}

		TextBoxBuilder &setConfigData(const TextBoxAttributes &textbox_config_data_)
		{
			tbcd_ = textbox_config_data_;
			return *this;
		}

		operator TextBox &&()
		{
			// tb_.Build(texts, fontFile, fontStyle, dsv);
			tb_.Build(context_, tbcd_);
			return std::move(tb_);
		}

	private:
		TextBox tb_;
		Context *context_;
		TextBoxAttributes tbcd_;
	};

	TextBoxBuilder TextBox::Builder(Context *context)
	{
		return TextBoxBuilder(context);
	}

	class TextBoxNavBarBuilder;

	class TextBoxNavBar : Context
	{
	public:
		TextBoxNavBar() = default;
		friend class TextBoxNavBarBuilder;
		SDL_FRect rect;
		static TextBoxNavBarBuilder Builder(Context *context);

		TextBoxNavBar &handleEvent(const SDL_Event &event)
		{
			if (event.type == SDL_EVENT_FINGER_DOWN)
			{
			}
			else if (event.type == SDL_EVENT_FINGER_MOTION)
			{
			}
			else if (event.type == SDL_EVENT_FINGER_UP)
			{
				const SDL_FPoint cf = {(event.tfinger.x * DisplayInfo::Get().RenderW) - rect.x,
									   (event.tfinger.y * DisplayInfo::Get().RenderH) - rect.y};
				std::size_t tmpSelectedTextViewIndex;
				const bool found = std::any_of(textViews.begin(), textViews.end(), [&](const Volt::TextBox &textView)
											   {
					if (SDL_PointInRectFloat(&cf, &textView.getBoundsConst())) {
						tmpSelectedTextViewIndex = textView.getId();
						return true;
					}
					else return false; });
				if (found && tmpSelectedTextViewIndex != selectedTextViewIndex)
				{
					UPDATE_SELECTED_TEXTVIEW = true;
					textViews[selectedTextViewIndex].updateTextColor(unselectedTextBgColor, {0, 0, 0, 0}, unselectedTextColor);
					textViews[tmpSelectedTextViewIndex].updateTextColor(selectedTextBgColor, {0, 0, 0, 0}, selectedTextColor);
					if (is_scrollable)
					{
						float distance_to_move = get_distance_to_move(tmpSelectedTextViewIndex);
						if (distance_to_move < 0.f)
							ACTN_MINUS = true;
						else
							ACTN_MINUS = false;
						animInterpolator.startWithDistance(std::fabs(distance_to_move));
					}
					selectedTextViewIndex = tmpSelectedTextViewIndex;
				}
			}
			return *this;
		}

		void udpdateSelectedText(const uint32_t &_index)
		{
			if (_index > textViews.size())
			{
				SDL_Log("update SelectedText called with out of range index. index: %d, textViews size: %lu", _index, textViews.size());
				return;
			}
			if (_index != selectedTextViewIndex)
			{
				if (onSelectedTextViewUpdateCallback != nullptr)
					onSelectedTextViewUpdateCallback(textViews[selectedTextViewIndex]);
				textViews[selectedTextViewIndex].updateTextColor({0, 0, 0, 0}, {0, 0, 0, 0}, unselectedTextColor);
				textViews[_index].updateTextColor({0, 0, 0, 0}, {0, 0, 0, 0}, selectedTextColor);
				selectedTextViewIndex = _index;
				const float sel_x = (rect.w / 2.f) - pv->to_cust(20.f, rect.w);
				const float distanceMoved = sel_x - textViews[_index].getBounds().x;

				// SDL_Log("DM: %f", distanceMoved);
				std::for_each(textViews.begin(), textViews.end(), [&](Volt::TextBox &textView)
							  { textView.updatePosBy(distanceMoved, 0.f); });
			}
			if (onSelectedTextViewUpdateCallback != nullptr)
				onSelectedTextViewUpdateCallback(textViews[selectedTextViewIndex]);
			UPDATE_SELECTED_TEXTVIEW = true;
		}

		TextBoxNavBar &Draw()
		{
			if (UPDATE_SELECTED_TEXTVIEW)
			{
				if (animInterpolator.isAnimating())
				{
					animInterpolator.update();
					dy = static_cast<int>(animInterpolator.getValue());
					dy = ACTN_MINUS ? (0 - dy) : dy;
				}
				else
				{
					UPDATE_SELECTED_TEXTVIEW = false;
					ACTN_MINUS = false;
					if (onSelectedTextViewUpdateCallback != nullptr)
						onSelectedTextViewUpdateCallback(textViews[selectedTextViewIndex]);
				}
				CacheRenderTarget rTargetCache(renderer);
				SDL_SetRenderTarget(renderer, texture.get());
				SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
				renderClear(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
				for (auto &textview : textViews)
				{
					if (UPDATE_SELECTED_TEXTVIEW)
						textview.updatePosBy(dy, 0.f);
					textview.Draw();
				}
				dy = 0.f;
				rTargetCache.release(renderer);
			}
			SDL_RenderTexture(renderer, texture.get(), nullptr, &rect);
			return *this;
		}

	private:
		TextBoxNavBar &registerOnSelectedTextViewUpdateCallback(std::function<void(const Volt::TextBox &)> _onSelectedTextViewUpdateCallback)
		{
			onSelectedTextViewUpdateCallback = std::move(_onSelectedTextViewUpdateCallback);
			return *this;
		}

		TextBoxNavBar &Build(const std::vector<std::string> &_texts, Volt::TextBoxAttributes _tbcd, const size_t &_defaultSelectedTextIndex = 0)
		{
			// if (_dest.w < 1 || _dest.h < 1)return *this;
			// Context::setContext(_context);
			/*rect = _dest;
			bgColor = _bgColor;
			selectedTextColor = _selected_text_color;
			unselectedTextColor = _unselected_text_color;*/

			/*spacing = pv->to_cust(5.f, rect.w);
			float i = 0.f, prev_tv_width = 0.f;*/
			// for (const auto& text : _texts) {
			//	textViews.emplace_back();
			//	textViews.back().setContext(getContext())
			//		.setShrinkToFit(true)
			//		.setId(i)
			//		.Build({ (i * spacing) + prev_tv_width, 0, rect.w/*to_cust(40.f, _dest.w)*/, rect.h },
			//			{ text, i == _defaultSelectedTextIndex ? selectedTextColor : unselectedTextColor, {0,0,0,0} },
			//			_fontFile, _fontStyle);
			//	prev_tv_width += textViews.back().rect.w;
			//	++i;
			// }
			_tbcd.shrinkToFit = true;
			_tbcd.gravity = Gravity::LEFT;
			_tbcd.edgeType = EdgeType::RADIAL;

			spacing = pv->to_cust(5.f, rect.w);
			float i = 0.f, prev_tv_width = 0.f;

			for (const auto &text : _texts)
			{
				_tbcd.textAttributes.bg_color = (i == _defaultSelectedTextIndex ? selectedTextBgColor : unselectedTextBgColor);
				_tbcd.textAttributes.text_color = (i == _defaultSelectedTextIndex ? selectedTextColor : unselectedTextColor);
				_tbcd.rect = {(i * spacing) + spacing + prev_tv_width, 0.f, rect.w /*to_cust(40.f, _dest.w)*/, rect.h};
				_tbcd.textAttributes.text = text;
				textViews.emplace_back();
				textViews.back().Build(getContext(), _tbcd).setId((uint32_t)(i));
				prev_tv_width += textViews.back().getBounds().w;
				++i;
			}

			// center the textviews
			/// TODO: a better approach would be to add up the two dx's so that theycan balance and use a single for each instead
			if (textViews.back().getBounds().x + textViews.back().getBounds().w > rect.w)
			{
				float dd = get_distance_to_move(_defaultSelectedTextIndex);
				std::for_each(textViews.begin(), textViews.end(), [&dd](Volt::TextBox &textView)
							  { textView.updatePosBy(dd, 0.f); });
				is_scrollable = true;
			}
			else
			{
				float dx = rect.w - (textViews.back().getBounds().x + textViews.back().getBounds().w);
				dx /= 2.f;
				std::for_each(textViews.begin(), textViews.end(), [&dx](Volt::TextBox &textView)
							  { textView.updatePosBy(dx, 0.f); });
			}
			selectedTextViewIndex = std::clamp(_defaultSelectedTextIndex, std::size_t(0), _texts.size());

			CacheRenderTarget rTargetCache(renderer);
			texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, rect.w, rect.h);
			SDL_SetRenderTarget(renderer, texture.get());
			SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
			renderClear(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
			// for (const auto& text : textViews) { text.Draw(); }
			rTargetCache.release(renderer);
			UPDATE_SELECTED_TEXTVIEW = true;
			return *this;
		}

		float get_distance_to_move(const int &tmpSelectedTextViewIndex)
		{
			float distance_to_move = 0.f;
			if (tmpSelectedTextViewIndex > selectedTextViewIndex)
			{
				if (tmpSelectedTextViewIndex + 1 < textViews.size())
				{
					if (textViews[tmpSelectedTextViewIndex + 1].getBounds().x + textViews[tmpSelectedTextViewIndex + 1].getBounds().w > rect.w - spacing)
					{
						distance_to_move = (textViews[tmpSelectedTextViewIndex + 1].getBounds().x + textViews[tmpSelectedTextViewIndex + 1].getBounds().w) - (rect.w - spacing);
						distance_to_move = 0.f - distance_to_move;
					}
				}
				else
				{
					if (textViews[tmpSelectedTextViewIndex].getBounds().x + textViews[tmpSelectedTextViewIndex].getBounds().w > rect.w - spacing)
					{
						distance_to_move = (textViews[tmpSelectedTextViewIndex].getBounds().x + textViews[tmpSelectedTextViewIndex].getBounds().w) - (rect.w - spacing);
						distance_to_move = 0.f - distance_to_move;
					}
				}
			}
			else if (tmpSelectedTextViewIndex < selectedTextViewIndex)
			{
				if (tmpSelectedTextViewIndex - 1 >= 0)
				{
					if (textViews[tmpSelectedTextViewIndex - 1].getBounds().x < spacing)
					{
						distance_to_move = std::fabs(textViews[tmpSelectedTextViewIndex - 1].getBounds().x) + spacing;
					}
				}
				else
				{
					// SDL_Log("else");
					if (textViews[tmpSelectedTextViewIndex].getBounds().x < 0.f)
					{
						distance_to_move = std::fabs(textViews[tmpSelectedTextViewIndex].getBounds().x) + spacing;
						// distance_to_move = std::clamp(distance_to_move, -1000000.f, spacing);
					}
				}
			}
			return distance_to_move;
		}

	private:
		bool UPDATE_SELECTED_TEXTVIEW = false, ACTN_MINUS = false;
		float dy = 0.f, spacing = 0.f;
		Interpolator animInterpolator;
		// int tmpSelectedTextViewIndex = -1;
		SDL_Color bgColor = {0, 0, 0, 0};
		SDL_Color selectedTextBgColor, selectedTextColor = {0xff, 0xff, 0xff, 0xff};
		SDL_Color unselectedTextBgColor, unselectedTextColor = {0xff, 0xff, 0xff, 127};
		int selectedTextViewIndex = 0;
		std::vector<Volt::TextBox> textViews;
		std::shared_ptr<SDL_Texture> texture;
		std::function<void(const Volt::TextBox &)> onSelectedTextViewUpdateCallback = nullptr;
		bool is_scrollable = false;
	};

	class TextBoxNavBarBuilder
	{
	public:
		TextBoxNavBarBuilder(Context *context)
		{
			textNavBar.setContext(context);
		}

		TextBoxNavBarBuilder &setRect(SDL_FRect _rect)
		{
			textNavBar.rect = _rect;
			return *this;
		}

		TextBoxNavBarBuilder &setDefaultTextIndex(const int &_dsv)
		{
			dsv = _dsv;
			return *this;
		}

		TextBoxNavBarBuilder &addTexts(const std::vector<std::string> &_texts)
		{
			texts = _texts;
			return *this;
		}

		TextBoxNavBarBuilder &setBgColor(SDL_Color _bgColor)
		{
			textNavBar.bgColor = _bgColor;
			return *this;
		}

		TextBoxNavBarBuilder &setTextBoxConfigData(Volt::TextBoxAttributes _tbcd)
		{
			tbcd = _tbcd;
			return *this;
		}

		TextBoxNavBarBuilder &setSelectedTextColor(SDL_Color _selectedTextBgColor, SDL_Color _selectedTextColor)
		{
			textNavBar.selectedTextBgColor = _selectedTextBgColor;
			textNavBar.selectedTextColor = _selectedTextColor;
			return *this;
		}

		TextBoxNavBarBuilder &setUnselectedTextColor(SDL_Color _unselectedTextBgColor, SDL_Color _unselectedTextColor)
		{
			textNavBar.unselectedTextBgColor = _unselectedTextBgColor;
			textNavBar.unselectedTextColor = _unselectedTextColor;
			return *this;
		}

		TextBoxNavBarBuilder &registerOnSelectedTextViewUpdate(std::function<void(const TextBox &)> _onSelectedTextViewUpdateCallback)
		{
			textNavBar.onSelectedTextViewUpdateCallback = _onSelectedTextViewUpdateCallback;
			return *this;
		}

		operator TextBoxNavBar &&()
		{
			textNavBar.Build(texts, tbcd, dsv);
			return std::move(textNavBar);
		}

	private:
		TextBoxNavBar textNavBar;
		std::vector<std::string> texts;
		int dsv = 0;
		TextBoxAttributes tbcd;
	};

	TextBoxNavBarBuilder TextBoxNavBar::Builder(Context *context)
	{
		return TextBoxNavBarBuilder(context);
	}

	struct EditBoxAttributes
	{
		SDL_FRect rect = {0.f, 0.f, 0.f, 0.f};
		SDL_FRect placeholderRect = {0.f, 0.f, 0.f, 0.f};
		TextAttributes textAttributes = {"", {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff}};
		TextAttributes placeholderTextAttributes = {"", {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff}};
		SDL_FRect margin = {0.f, 0.f, 100.f, 100.f};
		Font mem_font = Font::ConsolasBold;
		std::string fontFile;
		Font placeholder_mem_font = Font::ConsolasBold;
		std::string placeholderFontFile;
		FontStyle fontStyle = FontStyle::NORMAL;
		FontStyle defaultTxtFontStyle = FontStyle::NORMAL;
		EdgeType edgeType;
		uint32_t maxlines = 1;
		// max text size in code points
		uint32_t maxTextSize = 1024000;
		TextWrapStyle textWrapStyle = TextWrapStyle::MAX_CHARS_PER_LN;
		Gravity gravity = Gravity::LEFT;
		float cornerRadius = 0.f;
		int customFontstyle = 0x00;
		float lineSpacing = 0.f;
		float outline = 0.f;
		bool highlightOnHover = false;
		SDL_Color outlineColor = {0x00, 0x00, 0x00, 0x00};
		SDL_Color cusorColor = {0x00, 0x00, 0x00, 0xff};
		SDL_Color onHoverOutlineColor = {0x00, 0x00, 0x00, 0x00};
		SDL_Color onHoverBgColor = {0x00, 0x00, 0x00, 0x00};
		SDL_Color onHoverTxtColor = {0x00, 0x00, 0x00, 0x00};
		uint32_t cusorSpeed = 500;
	};

	class EditBoxBuilder;

	class EditBox : public Context, IView
	{
	public:
		using IView::bounds;
		using IView::getView;
		using IView::hide;
		using IView::isHidden;
		using IView::required;
		using IView::show;
		// using IView::id;

		EditBox() = default;
		int32_t id = (-1);
		friend class EditBoxBuilder;

		static EditBoxBuilder Builder(Context *context);

		EditBox &registerOnTextInputCallback(std::function<void(EditBox &)> _onTextInputCallback)
		{
			onTextInputCallback = _onTextInputCallback;
			return *this;
		}

		EditBox &registerOnTextInputFilterCallback(std::function<void(EditBox &, std::string &)> _onTextInputFilterCallback)
		{
			onTextInputFilterCallback = _onTextInputFilterCallback;
			return *this;
		}

		EditBox &Build(Context *_context, EditBoxAttributes &_attr)
		{
			Context::setContext(_context);
			adaptiveVsyncHD.setAdaptiveVsync(adaptiveVsync);
			bounds = _attr.rect;
			textRect = {
				bounds.x + to_cust(_attr.margin.x, bounds.w),
				bounds.y + to_cust(_attr.margin.y, bounds.h),
				to_cust(_attr.margin.w, bounds.w),
				to_cust(_attr.margin.h, bounds.h),
			};
			finalTextRect = textRect;

			cornerRadius = _attr.cornerRadius;
			textAttributes = _attr.textAttributes;
			fontAttributes = {_attr.fontFile, _attr.fontStyle, static_cast<uint8_t>(textRect.h)};
			maxCodePointsPerLn = static_cast<uint32_t>(textRect.w / (textRect.h / 2.f));
			place_holder_text = _attr.placeholderTextAttributes.text;

			cusor.setContext(getContext());
			cusor.setColor(_attr.cusorColor);
			cusor.setBlinkTm(_attr.cusorSpeed);
			cusor.setRect({textRect.x, textRect.y, to_cust(10.f, textRect.h), textRect.h});

			TTF_Font *tmpFont = nullptr;

			if (fontAttributes.font_file.empty())
			{
				Fonts[_attr.mem_font]->font_size = fontAttributes.font_size;
				fontAttributes.font_file = Fonts[_attr.mem_font]->font_name;
				tmpFont = FontSystem::Get().getFont(*Fonts[_attr.mem_font]);
			}

			maxTextSize = _attr.maxTextSize;
			if (not _attr.placeholderTextAttributes.text.empty())
			{
				dflTxtRect =
					{
						bounds.x + to_cust(_attr.placeholderRect.x, bounds.w),
						bounds.y + to_cust(_attr.placeholderRect.y, bounds.h),
						to_cust(_attr.placeholderRect.w, bounds.w),
						to_cust(_attr.placeholderRect.h, bounds.h),
					};

				FontAttributes plhFontAttr{
					_attr.placeholderFontFile.c_str(), _attr.defaultTxtFontStyle, static_cast<uint8_t>(dflTxtRect.h)};

				if (plhFontAttr.font_file.empty())
				{
					Fonts[_attr.placeholder_mem_font]->font_size = plhFontAttr.font_size;
					plhFontAttr.font_file = Fonts[_attr.placeholder_mem_font]->font_name;
					tmpFont = FontSystem::Get().getFont(*Fonts[_attr.placeholder_mem_font]);
				}

				FontSystem::Get().setFontAttributes({plhFontAttr.font_file.c_str(), plhFontAttr.font_style, plhFontAttr.font_size}, 0);
				auto textTex = FontSystem::Get().genTextTextureUnique(renderer, _attr.placeholderTextAttributes.text.c_str(), _attr.placeholderTextAttributes.text_color);

				int test_w, test_h;
				SDL_QueryTexture(textTex.value().get(), nullptr, nullptr, &test_w, &test_h);
				if ((int)(test_w) < dflTxtRect.w)
				{
					dflTxtRect.w = (float)(test_w);
				}
				if (static_cast<float>(test_h) >= dflTxtRect.h)
				{
					dflTxtRect.y += static_cast<float>(TTF_FontDescent(FontSystem::Get().getFont(_attr.placeholderFontFile, static_cast<int>(dflTxtRect.h))));
					dflTxtRect.h = static_cast<float>(test_h);
				}
				const SDL_FRect dflSrc = {0.f, 0.f, dflTxtRect.w, (float)test_h};
				const SDL_FRect dflDst = {0.f, 0.f, dflTxtRect.w, dflTxtRect.h};

				dflTxtTexture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
													SDL_TEXTUREACCESS_TARGET, static_cast<int>(dflTxtRect.w), static_cast<int>(dflTxtRect.h));
				CacheRenderTarget crt_(renderer);
				SDL_SetRenderTarget(renderer, dflTxtTexture.get());
				SDL_SetTextureBlendMode(dflTxtTexture.get(), SDL_BLENDMODE_BLEND);
				renderClear(renderer, 0, 0, 0, 0);
				SDL_RenderTexture(renderer, textTex.value().get(), &dflSrc, &dflDst);
				crt_.release(renderer);
			}
			outlineColor = _attr.outlineColor;
			onHoverOutlineColor = _attr.onHoverOutlineColor;
			highlightOnHover = _attr.highlightOnHover;
			onHoverBgColor = _attr.onHoverBgColor;
			outlineRect.Build(this, bounds, _attr.outline, cornerRadius, textAttributes.bg_color, _attr.outlineColor);

			return *this;
		}

		void Draw() override
		{
			outlineRect.Draw();
			// fillRoundedRectF(renderer, rect, cornerRadius, textAttributes.bg_color);
			if (not hasFocus)
			{
				if (internalText.empty())
					SDL_RenderTexture(renderer, dflTxtTexture.get(), nullptr, &dflTxtRect);
				else
					SDL_RenderTexture(renderer, txtTexture.get(), nullptr, &finalTextRect);
			}
			else
			{
				if (not internalText.empty())
				{
					SDL_RenderTexture(renderer, txtTexture.get(), nullptr, &finalTextRect);
				}
				cusor.Draw();
			}

			// return *this;
		}

		SDL_FRect &getRect()
		{
			return outlineRect.rect;
		}

		std::string getTextOrDefault()
		{
			return internalText.empty() ? place_holder_text : internalText;
		}

		std::string getText()
		{
			return internalText;
		}

		bool handleEvent() override
		{
			bool result_ = false;
			if (hasFocus and not SDL_TextInputActive())
				SDL_StartTextInput();
			switch (event->type)
			{
			case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
			{
				outlineRect.handleEvent();
				bounds =
					{
						DisplayInfo::Get().toUpdatedWidth(bounds.x),
						DisplayInfo::Get().toUpdatedHeight(bounds.y),
						DisplayInfo::Get().toUpdatedWidth(bounds.w),
						DisplayInfo::Get().toUpdatedHeight(bounds.h),
					};

				dflTxtRect =
					{
						DisplayInfo::Get().toUpdatedWidth(dflTxtRect.x),
						DisplayInfo::Get().toUpdatedHeight(dflTxtRect.y),
						DisplayInfo::Get().toUpdatedWidth(dflTxtRect.w),
						DisplayInfo::Get().toUpdatedHeight(dflTxtRect.h),
					};

				finalTextRect =
					{
						DisplayInfo::Get().toUpdatedWidth(finalTextRect.x),
						DisplayInfo::Get().toUpdatedHeight(finalTextRect.y),
						DisplayInfo::Get().toUpdatedWidth(finalTextRect.w),
						DisplayInfo::Get().toUpdatedHeight(finalTextRect.h),
					};

				auto tmpRct = cusor.getRect();
				tmpRct =
					{
						DisplayInfo::Get().toUpdatedWidth(tmpRct.x),
						DisplayInfo::Get().toUpdatedHeight(tmpRct.y),
						DisplayInfo::Get().toUpdatedWidth(tmpRct.w),
						DisplayInfo::Get().toUpdatedHeight(tmpRct.h),
					};
				cusor.setRect(tmpRct);
				// fontAttributes.font_size = DisplayInfo::Get().toUpdatedHeight(fontAttributes.font_size);
				result_ = true;
			}
			break;
			case SDL_EVENT_FINGER_DOWN:
			{
				if (onClick(event->tfinger.x * DisplayInfo::Get().RenderW, event->tfinger.y * DisplayInfo::Get().RenderH) and not hasFocus)
				{
					hasFocus = true;
					adaptiveVsyncHD.startRedrawSession();
					SDL_StartTextInput();
				}
				else
				{
					if (hasFocus)
					{
						SDL_StopTextInput(), hasFocus = false, adaptiveVsyncHD.stopRedrawSession();
						if (highlightOnHover)
						{
							outlineRect.outline_color = outlineColor;
							outlineRect.color = textAttributes.bg_color;
						}
					}
				}
			}
			break;
			/*case SDL_EVENT_FINGER_MOTION:
			{
				if (onClick(_event.tfinger.x * DisplayInfo::Get().RenderW, _event.tfinger.y * DisplayInfo::Get().RenderH)){
					if(highlightOnHover){
						outlineRect.outline_color = onHoverOutlineColor;
					}
				}else{
					if (highlightOnHover) {
						outlineRect.outline_color = outlineColor;
					}
				}
			}
			break;*/
			case SDL_EVENT_MOUSE_MOTION:
			{
				if (onClick(event->motion.x, event->motion.y))
				{
					if (highlightOnHover)
					{
						outlineRect.outline_color = onHoverOutlineColor;
						outlineRect.color = onHoverBgColor;
					}
				}
				else
				{
					if (highlightOnHover and not hasFocus)
					{
						outlineRect.outline_color = outlineColor;
						outlineRect.color = textAttributes.bg_color;
					}
				}
			}
			break;
			case SDL_EVENT_FINGER_UP:
			{
				if (onClick(event->tfinger.x * DisplayInfo::Get().RenderW, event->tfinger.x * DisplayInfo::Get().RenderH))
				{
					// hasFocus = true;
				}
				else
				{
					// if (hasFocus)SDL_StopTextInput(), hasFocus = false;
				}
			}
			break;
			case SDL_EVENT_KEY_DOWN:
			{
				if (hasFocus)
				{
					// SDL_Log("key down");
					cusor.m_start = SDL_GetTicks();
					if (event->key.keysym.scancode == SDL_SCANCODE_BACKSPACE and not internalText.empty())
					{
						internalText.erase(internalText.size() - (inputSize.back())),
							inputSize.pop_back();
						updateTextTexture("");
						if (onTextInputCallback)
							onTextInputCallback(*this);
					}
					else if (event->key.keysym.scancode == SDL_SCANCODE_SELECT)
					{
					}
				}
			}
			break;
			case SDL_EVENT_TEXT_INPUT:
			{
				if (hasFocus)
				{
					// SDL_Log("txt input");
					cusor.m_start = SDL_GetTicks();
					std::string newText = event->text.text;
					if (onTextInputFilterCallback)
						onTextInputFilterCallback(*this, newText);
					if (not newText.empty())
					{
						if (inputSize.size() + 1 <= maxTextSize)
						{
							inputSize.emplace_back(newText.size());
							updateTextTexture(newText);
							if (onTextInputCallback)
								onTextInputCallback(*this);
						}
					}
				}
			}
			break;
			}

			return result_;
		}

		EditBox &setOnHoverOutlineColor(const SDL_Color &_color)
		{
			outlineRect.outline_color = _color;
			onHoverOutlineColor = _color;
			return *this;
		}

		EditBox &setOutlineColor(const SDL_Color &_color)
		{
			outlineRect.outline_color = _color;
			outlineColor = _color;
			return *this;
		}

		EditBox &clearAndSetText(const std::string &newText)
		{
			inputSize.clear();
			internalText.clear();
			for (const auto &ch_ : newText)
			{
				if (inputSize.size() + 1 <= maxTextSize)
				{
					inputSize.emplace_back(1);
				}
				else
				{
					updateTextTexture(newText.substr(0, inputSize.size()));
					return *this;
				}
			}
			updateTextTexture(newText);
			// cusor.setPosX(textRect.x + float(tmpW));
			return *this;
		}

	protected:
		template <typename T>
		bool onClick(T x, T y, unsigned short axis = 0)
		{
			[[likely]] if (axis == 0)
			{
				if (x < bounds.x or x > (bounds.x + bounds.w) or y < bounds.y ||
					y > (bounds.y + bounds.h))
					return false;
			}
			else if (axis == 1 /*x-axis only*/)
			{
				if (x < bounds.x or x > (bounds.x + bounds.w))
					return false;
			}
			else if (axis == 2 /*y-axis only*/)
			{
				if (y < bounds.y or y > (bounds.y + bounds.h))
					return false;
			}
			return true;
		}

	protected:
		void updateTextTexture(const std::string &_newText)
		{
			internalText += _newText;
			if (internalText.empty())
			{
				cusor.setPosX(textRect.x);
				return;
			}
			std::string visibleText = internalText;
			if (inputSize.size() <= maxCodePointsPerLn)
				visibleText = internalText;
			else
			{
				const int correctedTextSize = std::reduce(inputSize.begin() + (inputSize.size() - maxCodePointsPerLn), inputSize.end());
				visibleText = internalText.substr(internalText.size() - correctedTextSize, internalText.size());
			}

			CacheRenderTarget rTargetCache(renderer);
			txtTexture.reset();
			auto textTex = FontSystem::Get().setFontAttributes(fontAttributes, 0).genTextTextureUnique(renderer, visibleText.c_str(), textAttributes.text_color);
			int tmpW, tmpH;
			SDL_QueryTexture(textTex.value().get(), nullptr, nullptr, &tmpW, &tmpH);
			tmpW = std::clamp(tmpW, 0, (int)(textRect.w));
			txtTexture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
											 SDL_TEXTUREACCESS_TARGET, tmpW, textRect.h);
			SDL_SetTextureBlendMode(txtTexture.get(), SDL_BLENDMODE_BLEND);
			SDL_SetRenderTarget(renderer, txtTexture.get());

			renderClear(renderer, 0, 0, 0, 0);
			SDL_RenderTexture(renderer, textTex.value().get(), nullptr, nullptr);
			rTargetCache.release(renderer);
			cusor.setPosX(textRect.x + float(tmpW));
			finalTextRect.w = tmpW;
		}

	protected:
		float cornerRadius = 0.f;
		std::shared_ptr<SDL_Texture> txtTexture;
		std::shared_ptr<SDL_Texture> dflTxtTexture;
		std::function<void(EditBox &)> onTextInputCallback;
		std::function<void(EditBox &)> onKeyPressEnter;
		std::function<void(EditBox &, std::string &)> onTextInputFilterCallback;
		TextAttributes textAttributes;
		FontAttributes fontAttributes;
		std::vector<uint32_t> inputSize;
		uint32_t maxTextSize = 100;

		SDL_FRect dflTxtRect;
		// SDL_FRect cusorRect;
		SDL_FRect textRect;
		SDL_FRect finalTextRect;
		std::string internalText, place_holder_text;
		RectOutline outlineRect;
		bool hasFocus = false;
		// SDL_Color cusorColor;
		Cursor cusor;
		uint32_t maxCodePointsPerLn = 1;
		AdaptiveVsyncHandler adaptiveVsyncHD, updAdaptiveVsyncHD;
		bool highlightOnHover = false;
		SDL_Color onHoverOutlineColor = {0x00, 0x00, 0x00, 0x00};
		SDL_Color onHoverBgColor = {0x00, 0x00, 0x00, 0x00};
		SDL_Color onHoverTxtColor = {0x00, 0x00, 0x00, 0x00};
		SDL_Color outlineColor = {0x00, 0x00, 0x00, 0x00};
	};

	class EditBoxBuilder : Context
	{
	public:
		EditBoxBuilder() = default;

		EditBoxBuilder(Context *_context)
		{
			setContext(_context);
		}
		operator EditBox &&()
		{
			editBox.Build(this, editBoxAttr);
			return std::move(editBox);
		}

	private:
		EditBox editBox;
		EditBoxAttributes editBoxAttr;
	};

	EditBoxBuilder EditBox::Builder(Context *context)
	{
		return EditBoxBuilder(context);
	}

	namespace Expr
	{
		struct SliderAttributes
		{
			SDL_FRect rect = {};
			float min_val = 0.f, max_val = 1.f, start_val = 0.f, knob_radius = 0.f;
			bool show_knob = true;
			SDL_Color bg_color = {50, 50, 50, 255};
			SDL_Color level_bar_color = {255, 255, 255, 255};
			SDL_Color knob_color = {255, 255, 255, 255};
			Orientation orientation = Orientation::HORIZONTAL;
		};

		class Slider : public Context, IView
		{
		public:
			using IView::getView;

		public:
			Slider &Build(Context *_context, SliderAttributes _attr)
			{
				setContext(_context);
				knob.setContext(_context);
				knob.Build(0.f, 0.f, _attr.knob_radius, _attr.knob_color);
				rect = _attr.rect;
				lvl_rect = rect;
				bg_color = _attr.bg_color;
				lvl_bar_color = _attr.level_bar_color;
				orientation = _attr.orientation;
				show_knob = _attr.show_knob;
				updateMinMaxValues(_attr.min_val, _attr.max_val, _attr.start_val);
				return *this;
			}

			bool handleEvent() override
			{
				bool result = false;
				switch (event->type)
				{
				case SDL_EVENT_MOUSE_BUTTON_DOWN:
					if (pointInBound(event->motion.x, event->motion.y))
					{
						key_down = true, result = true;
						if (Orientation::VERTICAL == orientation)
							updateValue(screenToWorld(event->motion.y - rect.y));
						else if (Orientation::HORIZONTAL == orientation)
							updateValue(screenToWorld(event->motion.x - rect.x));
					}
					break;
				case SDL_EVENT_MOUSE_MOTION:
					if (key_down)
					{
						result = true;
						if (Orientation::VERTICAL == orientation)
							updateValue(screenToWorld(event->motion.y - rect.y));
						else if (Orientation::HORIZONTAL == orientation)
							updateValue(screenToWorld(event->motion.x - rect.x));
					}
					break;
				case SDL_EVENT_MOUSE_BUTTON_UP:
					key_down = false;
					break;
				default:
					break;
				}
				return result;
			}

			void Draw() override
			{
				if (Orientation::HORIZONTAL == orientation)
				{
					SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
					SDL_RenderFillRect(renderer, &rect);
					SDL_SetRenderDrawColor(renderer, lvl_bar_color.r, lvl_bar_color.g, lvl_bar_color.b, lvl_bar_color.a);
					SDL_RenderFillRect(renderer, &lvl_rect);
				}
				else if (Orientation::VERTICAL == orientation)
				{
					SDL_SetRenderDrawColor(renderer, lvl_bar_color.r, lvl_bar_color.g, lvl_bar_color.b, lvl_bar_color.a);
					SDL_RenderFillRect(renderer, &rect);
					SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
					SDL_RenderFillRect(renderer, &lvl_rect);
				}

				if (show_knob)
				{
					if (Orientation::HORIZONTAL == orientation)
					{
						knob.dest.x = lvl_rect.x + lvl_rect.w - (knob.dest.w / 2.f);
						knob.dest.y = rect.y + (rect.h / 2.f) - (knob.dest.h / 2.f);
					}
					else if (Orientation::VERTICAL == orientation)
					{
						knob.dest.x = (lvl_rect.x + (lvl_rect.w / 2.f)) - (knob.dest.w / 2.f);
						knob.dest.y = (lvl_rect.y + lvl_rect.h) - (knob.dest.w / 2.f);
					}
					knob.Draw(renderer);
				}
			}

			Slider &registerOnValueUpdateCallback(std::function<void(Slider &)> onValUpdateClbk)
			{
				onValUpdate = onValUpdateClbk;
				return *this;
			}

			auto getCurrentValue()
			{
				if (Orientation::VERTICAL == orientation)
					return max_val - (current_val - min_pad);
				return current_val - min_pad;
			}

			auto getMinValue() { return min_val - min_pad; }
			auto getMaxValue() { return max_val - max_pad; }
			auto isKeyDown() noexcept { return key_down; }
			auto getLevelLength()
			{
				if (Orientation::HORIZONTAL == orientation)
					return lvl_rect.w;
				else if (Orientation::VERTICAL == orientation)
					return rect.h - lvl_rect.h;
				else if (Orientation::ANGLED == orientation)
					return 0.f;
			}

			Slider &updateValue(float _val)
			{
				current_val = std::clamp(_val + min_pad, min_val, max_val);
				if (Orientation::HORIZONTAL == orientation)
					lvl_rect.w = (current_val * ratio);
				else if (Orientation::VERTICAL == orientation)
					lvl_rect.h = (current_val * ratio);
				if (onValUpdate)
					onValUpdate(*this);
				return *this;
			}

			Slider &updateMinMaxValues(float _min_val, float _max_val, float _current_val)
			{
				min_val = _min_val;
				max_val = _max_val;
				// possible integer overflow bug if min/max val > INT_MAX/2
				// should test and add a static assert
				if (min_val < 0.f)
					min_val = fabs(min_val), min_pad = min_val * 2.f;
				if (max_val < 0.f)
					max_val = fabs(max_val), max_pad = max_val * 2.f;
				max_val += min_pad;
				length = max_val - min_val;

				if (Orientation::HORIZONTAL == orientation)
					ratio = rect.w / length;
				else if (Orientation::VERTICAL == orientation)
					ratio = rect.h / length;
				return updateValue(_current_val);
			}

			void updatePosBy(float dy, float dx) override
			{
				rect.x += dx;
				rect.y += dy;
				lvl_rect.x += dx;
				lvl_rect.y += dy;
			}

		private:
			bool pointInBound(const float &x, const float &y) const noexcept
			{
				if (Orientation::VERTICAL == orientation)
				{
					if ((x >= knob.dest.x) && (x < knob.dest.x + knob.dest.w) && (y >= rect.y) && (y < rect.y + rect.h))
						return true;
				}
				else if (Orientation::HORIZONTAL == orientation)
				{
					if ((x >= rect.x) && (x < rect.x + rect.w) && (y >= knob.dest.y) && (y < knob.dest.y + knob.dest.h))
						return true;
				}
				else if (Orientation::ANGLED == orientation)
					return false;
				return false;
			}

			float screenToWorld(const float val) const
			{
				if (Orientation::HORIZONTAL == orientation)
					return (val * length) / rect.w;
				else if (Orientation::VERTICAL == orientation)
					return (val * length) / rect.h;
				else
					return 0.f;
			}

		private:
			float min_val = 0.f, max_val = 0.f, current_val = 0.f, min_pad = 0.f, max_pad = 0.f;
			SDL_Color bg_color = {50, 50, 50, 255};
			SDL_Color lvl_bar_color = {255, 255, 255, 255};
			Orientation orientation = Orientation::HORIZONTAL;
			Circle knob;
			SDL_FRect rect, lvl_rect;
			bool show_knob = true, key_down = false;
			float ratio = 0.f;
			float length = 0.f;
			std::function<void(Slider &)> onValUpdate = nullptr;
		};
	};

	struct SliderAttributes
	{
		float x = 0.f, y = 0.f, w = 0.f, length = 0.f, min_value = 0.f, max_value = 0.f, current_value = 0.f, knob_radius = 0.f;
		SDL_Color bar_color = {};
		SDL_Color level_bar_color = {};
		SDL_Color knob_color = {};
		bool show_knob = true;
		Orientation orientation = Orientation::VERTICAL;
	};

	class Slider : public Context
	{
	public:
		Slider() : x(0.f), y(0.f), w(0.f), length(0.f), min_val(0.f), max_val(0.f),
				   current_val(0.f),
				   bar_color({90, 90, 90, 0xff}),
				   level_bar_color({0xff, 0xff, 0xff, 0xff}),
				   knob_color({0xff, 0xff, 0xff, 0xff}) {}

		Slider &SetUp(Context *_context, SliderAttributes _attr)
		{
			setContext(_context);
			knob.setContext(_context);
			knob_color = _attr.knob_color;
			bar_color = _attr.bar_color;
			level_bar_color = _attr.level_bar_color;
			show_knob = _attr.show_knob;
			radius = _attr.knob_radius;
			orientation = _attr.orientation;
			w = _attr.w;
			if (w == 0.f)
				w = DisplayInfo::Get().pw(1.5f);
			if (radius == 0.f)
				radius = w;
			radius += 1;
			x = _attr.x, y = _attr.y;
			length = _attr.length;
			knob.Build(0, 0, radius, knob_color);
			if (orientation == Orientation::VERTICAL)
			{
				knob.dest.x = (x + (w / 2.f)) - radius + 1.f;
				knob.dest.y = y + length - radius - current_bar_val;
			}
			else if (orientation == Orientation::HORIZONTAL)
			{
				knob.dest.x = (x + (w / 2.f)) - radius + 1.f;
				knob.dest.y = y + (w / 2.f) - radius + 1.f;
			}
			return *this;
		}

		Slider &Build(float _min_val, float _max_val)
		{
			min_val = _min_val, max_val = _max_val;
			if (min_val >= 0)
				dt_min_max = max_val - min_val;
			else
				dt_min_max = max_val + fabs(min_val);
			int ol = length - (radius * 2);
			range_ratio = dt_min_max / length;
			current_val = min_val;
			ready = true;
			return *this;
		}

		Slider &SetShowKnob(const bool &show_knob_)
		{
			this->show_knob = show_knob_;
			return *this;
		}

		Slider &SetKnobColor(const SDL_Color &knob_color_)
		{
			this->knob_color = knob_color_;
			return *this;
		}

		Slider &SetKnobBorderColor(const SDL_Color &knob_border_color_)
		{
			this->knob.border_color = knob_border_color_;
			return *this;
		}

		Slider &SetLevelBarColor(const SDL_Color &level_bar_color_)
		{
			this->level_bar_color = level_bar_color_;
			return *this;
		}

		Slider &SetBarColor(const SDL_Color &bar_color_)
		{
			this->bar_color = bar_color_;
			return *this;
		}

		Slider &UpdateValue(const float &value)
		{
			if (value < min_val)
				current_val = min_val, SDL_Log("slider value is less than min_val");
			else if (value > max_val)
				current_val = max_val, SDL_Log("slider value is greater than max_val");
			else
				current_val = value;
			current_bar_val = (current_val - min_val) / range_ratio;
			update(current_bar_val);
			return *this;
		}

		Slider &SetKnobRadius(const float &knob_radius_)
		{
			this->radius = knob_radius_;
			return *this;
		}

		Slider &SetW(const float &w_)
		{
			this->w = w_;
			return *this;
		}

		float GetValue() const
		{
			return this->current_val;
		}

		float GetLvlBarLength()
		{
			return this->current_bar_val;
		}

		SDL_FRect GetLevelPos() const
		{
			return knob.dest;
		}

		bool OnClick(const float &x_, const float &y_)
		{
			if (orientation == Orientation::VERTICAL)
			{
				if (x_ >= x - radius + (w / 2) && x_ <= x + w + (w / 2) && y_ >= y &&
					y_ <= y + length)
					return true;
			}
			else if (orientation == Orientation::HORIZONTAL)
			{
				if (x_ >= x && x_ <= x + length &&
					y_ >= y && y_ <= y + w)
					return true;
			}
			return false;
		}

		bool handle_event(const SDL_Event &e)
		{
			bool RESULT = false;
			switch (e.type)
			{
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				if (OnClick(e.motion.x, e.motion.y))
				{
					key_down = true, RESULT = true;
					if (orientation == Orientation::VERTICAL)
						update(length - (e.motion.y - y));
					else if (orientation == Orientation::HORIZONTAL)
						update(e.motion.x - x);
				}
				break;
			case SDL_EVENT_MOUSE_MOTION:
				if (key_down)
				{
					RESULT = true;
					if (orientation == Orientation::VERTICAL)
						update(length - (e.motion.y - y));
					else if (orientation == Orientation::HORIZONTAL)
						update(e.motion.x - x);
				}
				break;
			case SDL_EVENT_MOUSE_BUTTON_UP:
				key_down = false;
				break;
			default:
				break;
			}

			return RESULT;
		}

		void Draw() const
		{
			if (ready)
			{
				if (orientation == Orientation::VERTICAL)
				{
					fillRoundedRectF(renderer, {x, y, w, length}, 100, bar_color);
					if (current_bar_val > 1)
						fillRoundedRectF(renderer, {x, y + length - current_bar_val, w, current_bar_val}, 100, level_bar_color);
				}
				else if (orientation == Orientation::HORIZONTAL)
				{
					fillRoundedRectF(renderer, {x, y, length, w}, 100, bar_color);
					if (current_bar_val > 1)
						fillRoundedRectF(renderer, {x, y, current_bar_val, w}, 100, level_bar_color);
				}
				if (show_knob)
					knob.Draw(renderer);
			}
		}

	private:
		SDL_Color bar_color, level_bar_color, knob_color;
		Orientation orientation;
		Circle knob;
		SDL_Rect dest;
		float x, y, w, length, radius, range_ratio, min_val, max_val, current_val, current_bar_val, dt_min_max;
		bool key_down = false, show_knob, ready = false;

		void update(const float &val)
		{
			if (orientation == Orientation::VERTICAL)
			{
				current_bar_val = val;
				if (current_bar_val > length)
					current_bar_val = length;
				if (current_bar_val < 0)
					current_bar_val = 0;
				knob.dest.y = y + length - radius - current_bar_val;
			}
			else if (orientation == Orientation::HORIZONTAL)
			{
				current_bar_val = val;
				if (current_bar_val > length)
					current_bar_val = length;
				if (current_bar_val < 0)
					current_bar_val = 0;
				knob.dest.x = ((x)-radius) + current_bar_val;
			}

			current_val = min_val + (current_bar_val * range_ratio);

			// SDL_Log("CURRENT_VAL: %f", current_val);
		}
	};

	class ToggleButton : public Context, IView
	{
	public:
		using IView::bounds;
		ToggleButton() : checked(false), clicked(false),
						 unchecked_color({100, 100, 100, 0xff}),
						 checked_color({39, 169, 55, 0xff}),
						 v_padding(0), h_padding(0), knob_color({0xff, 0xff, 0xff, 0xff}) {}

		bool checked, clicked;
		// SDL_FRect dest;
		Circle knob;
		float knob_x_unchecked, knob_x_checked, v_padding, h_padding, knob_r;
		SDL_Color unchecked_color, checked_color, knob_color;

		ToggleButton &setContext(Context *context_)
		{
			Context::setContext(context_);
			this->knob.setContext(context_);
			return *this;
		}

		ToggleButton &setOnClickedCallback(std::function<void()> on_clicked_callback)
		{
			onClickedCallback = on_clicked_callback;
			return *this;
		}

		ToggleButton &SetRect(const SDL_FRect &dest_)
		{
			this->bounds = dest_;
			return *this;
		}

		ToggleButton &SetOffColor(const SDL_Color &off_color_)
		{
			this->unchecked_color = off_color_;
			return *this;
		}

		ToggleButton &SetOnColor(const SDL_Color &on_color_)
		{
			this->checked_color = on_color_;
			return *this;
		}

		ToggleButton &SetKnobColor(const SDL_Color &knob_color_)
		{
			this->knob_color = knob_color_;
			return *this;
		}

		void Build()
		{
			v_padding = (to_cust(5.f, bounds.h));
			h_padding = to_cust(2.f, bounds.w);
			knob_r = to_cust(45.f, bounds.h);
			knob_x_unchecked = bounds.x + h_padding + to_cust(2.f, bounds.h);
			knob_x_checked = bounds.x + bounds.w + h_padding - to_cust(98.f, bounds.h);
			knob.Build(checked ? knob_x_checked : knob_x_unchecked,
					   bounds.y + v_padding, knob_r, knob_color);
		}

		bool OnClick(const float &x_, const float &y_)
		{
			if (x_ >= bounds.x && x_ <= bounds.x + bounds.w && y_ >= bounds.y &&
				y_ <= bounds.y + bounds.h)
				return true;
			return false;
		}

		bool HandleEvent()
		{
			bool RESULT = false;
			switch (event->type)
			{
			case SDL_EVENT_MOUSE_BUTTON_DOWN:
				if (OnClick(event->motion.x, event->motion.y))
					clicked = true;
				break;
			case SDL_EVENT_MOUSE_MOTION:
				if (!OnClick(event->motion.x, event->motion.y))
					clicked = false;
				break;
			case SDL_EVENT_MOUSE_BUTTON_UP:
				if (OnClick(event->motion.x, event->motion.y) && clicked)
				{
					if (checked)
						checked = false, this->knob.dest.x = this->knob_x_unchecked;
					else
						checked = true, this->knob.dest.x = knob_x_checked;
					RESULT = true;
					if (onClickedCallback != nullptr)
						onClickedCallback();
				}
				this->clicked = false;
				break;
			default:
				break;
			}
			return RESULT;
		}

		void Draw()
		{
			/*fillRoundedRect(renderer,{dest.x, dest.y, dest.w, dest.h}, 100,
							checked ? checked_color : unchecked_color);*/
			drawRoundedRectF(renderer, {bounds.x, bounds.y, bounds.w, bounds.h}, 100.f,
							 checked ? checked_color : unchecked_color);
			this->knob.Draw(renderer);
			// DrawCircle(renderer, knob.dest.x + knob.dest.w / 2, knob.dest.y + knob.dest.h / 2, knob.dest.w / 2, { 0,0,0xff,0xff });
		}

	private:
		std::function<void()> onClickedCallback = nullptr;
	};

	class ScrollBar : public UIWidget, public Context
	{
	private:
		bool auto_hide_;
		Uint64 auto_hide_time_;
		unsigned short free_axis_;
		Orientation orientation_;
		Uint64 then_;
		Uint8 alpha_;
		float range_min_, range_max_, cache_x_, ratio_, value_, reference_;
		SDL_FRect range;
		short first_t;

	public:
		ScrollBar() {}

		ScrollBar(const float &x, const float &y, const float &w, const float &h,
				  const int &range_min, const int &range_max, const unsigned short &free_axis,
				  const SDL_Color &color, const bool &auto_hide = true)
		{
			create(x, y, w, h, range_min, range_max, free_axis, color, auto_hide);
		}

		ScrollBar &setContext(Context *context_)
		{
			Context::setContext(context_);
			return *this;
		}

		void create(const float &x, const float &y, const float &w, const float &h,
					const int &range_min, const int &range_max, const unsigned short &free_axis,
					const SDL_Color &color, const bool &auto_hide = true)
		{
			first_t = 0;
			this->dest = {x, y, w,
						  h},
			this->range_min_ = range_min, this->range_max_ = range_max, this->auto_hide_ = auto_hide, this->auto_hide_time_ = 3000, this->free_axis_ = free_axis, this->value_ = 0.f;
			this->range = {x + 4.f, (float)range_min, w - 8.f, (float)(range_max - range_min)};
			cache_x_ = x;
			// anim_cnt = range.w;
			alpha_ = 100;
			this->texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
												SDL_TEXTUREACCESS_TARGET, w, h);
			SDL_SetTextureBlendMode(this->texture.get(), SDL_BLENDMODE_BLEND);
			CacheRenderTarget rTargetCache(renderer);
			SDL_SetRenderTarget(renderer, this->texture.get());
			renderClear(renderer, 0, 0, 0, 0);
			fillRoundedRectF(renderer, {0, 0, w, h}, 100, color);
			rTargetCache.release(renderer);
		}

		void draw()
		{
			// if(first_t==0)this->then_ = SDL_GetTicks(), first_t++;
			Uint64 dt = SDL_GetTicks() - then_;

			if (dt <= 500)
			{
				range.x = cache_x_ + 4.f, dest.x = cache_x_;
				dest.y = range_min_ + value_;
				alpha_ = 100;
				if (dest.y < range_min_)
					dest.y = range_min_;
				SDL_SetRenderDrawColor(renderer, 100, 100, 100, 100);
				SDL_RenderFillRect(renderer, &range);
				// drawBorderedRect(renderer, range.x, range.y, range.h, range.w, 100, {0xff, 0xff, 0xff, 100});
				UIWidget::Draw(renderer);
			}
			else if (dt > 500 && dest.x <= DisplayInfo::Get().RenderW)
			{
				alpha_ -= 5;
				if (alpha_ <= 5)
					alpha_ = 5, dest.x += 2, range.x += 2;

				SDL_SetRenderDrawColor(renderer, 100, 100, 100, alpha_);
				SDL_RenderFillRect(renderer, &range);
				// drawBorderedRect(renderer, range.x, range.y, range.h, range.w, 95, {0xff, 0xff, 0xff, alp});
				UIWidget::Draw(renderer);
			}
			/*if (dest.x>DisplayMetrics::Get().RenderW)
			{
				range.x = cache_x + 4.f, dest.x = cache_x;
				return;
			}*/
			/*else if (dt > 3000)
			{
				range.x = cache_x + 4.f, dest.x = cache_x;
				return;
			}*/
		}

		void scroll_neg(const float &val_moved)
		{
			// ratio_ = reference_ / range.h;
			float res = (val_moved * range.h) / reference_;
			value_ -= res;
		}

		void scroll_pos(const float &val_moved)
		{
			// ratio_ = reference_ / range.h;
			float res = (val_moved * range.h) / reference_;
			// SDL_Log("ref: %f range: %f res: %f val_m: %f", reference_,range.h,res,val_moved);
			value_ += res;
		}

		void set_reference(const float &reference)
		{
			reference_ = reference;
		}

		void set_range_max(const float &range_max)
		{
			if (range_max < range_max_)
			{
				float diff = range_max_ - range_max;
				// value_-=diff;
			}
			else if (range_max > range_max_)
			{
				float diff = range_max - range_max_;
				// value_+=diff;
			}
			else if (range_max == range_max_)
				return;
			range_max_ = range_max;
			range.h = range_max_ - range_min_;
		}

		void set_then(const Uint64 &then)
		{
			this->then_ = then;
		}
	};

	class Icon : public Context
	{
	public:
		Icon &setContext(Context *context_)
		{
			Context::setContext(context_);
			return *this;
		}

		void create(const std::string &name, const SDL_FRect &rc)
		{
			if (rc.w <= 0.f || rc.h <= 0.f)
			{
				SDL_Log("Error Loading Icon - Invalid Rect Values");
				return;
			}
			SDL_Surface *ldSurface = IMG_Load(name.c_str());
			SDL_Texture *load_icn = SDL_CreateTextureFromSurface(renderer, ldSurface);
			SDL_DestroySurface(ldSurface);
			CacheRenderTarget rTargetCache(renderer);
			texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
										  SDL_TEXTUREACCESS_TARGET, (int)rc.w,
										  (int)rc.h);
			SDL_SetRenderTarget(renderer, this->texture.get());
			SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
			renderClear(renderer, 0x0, 0x0, 0x0, 0x0);
			SDL_RenderTexture(renderer, load_icn, nullptr, nullptr);
			rTargetCache.release(renderer);
			this->dest = rc;
			SDL_DestroyTexture(load_icn);
			load_icn = nullptr;
		}

		void create(const std::string &name)
		{
			SDL_Surface *ldSurface = IMG_Load(name.c_str());
			texture = CreateSharedTextureFromSurface(renderer, ldSurface);
			SDL_DestroySurface(ldSurface);
			// dest = {0.f, 0.f, 0.f, 0.f};
		}

		void registerCustomLazyImageLoaderCallBack(std::function<SDL_Surface *()> _CustomLazyImageLoaderCallBack)
		{
			CustomLazyImageLoaderCallBack_ = _CustomLazyImageLoaderCallBack;
		}

		void createLazy(const std::string &name, const SDL_FRect &rc)
		{
			if (rc.w <= 0.f || rc.h <= 0.f)
			{
				SDL_Log("Error Loading Icon - Invalid Rect Values");
				return;
			}
			// lazyLoadFuture.
		}

		bool onClick(const float &x, const float &y) const
		{
			if (x > dest.x && x < (dest.x + dest.w) && y > dest.y && y < dest.y + dest.h)
				return true;
			return false;
		}

		inline void setDestRect(const SDL_FRect &rect)
		{
			this->dest = rect;
		}

		SDL_FRect &getRect()
		{
			return this->dest;
		}

		inline void draw() const
		{
			if (texture.get() != nullptr)
				SDL_RenderTexture(renderer, texture.get(), nullptr, &dest);
		}

		/*Icon *getClass() {
			return this;
		}*/

		Icon &setOnClickedCallback(std::function<void()> on_clicked_callback)
		{
			onClickedCallback = std::move(on_clicked_callback);
			return *this;
		}

		Icon &handleEvent(const SDL_Event &event)
		{
			if (event.type == SDL_EVENT_FINGER_UP)
			{
				if (onClick(event.tfinger.x * DisplayInfo::Get().RenderW, event.tfinger.y * DisplayInfo::Get().RenderH) &&
					onClickedCallback != nullptr)
					onClickedCallback();
			}
			return *this;
		}

		SDL_FRect dest;

	private:
		std::shared_future<SDL_Surface *> lazyLoadFuture;
		SharedTexture texture;
		SDL_Texture *defaultTexture_ = nullptr;
		std::function<void()> onClickedCallback = nullptr;
		std::function<SDL_Surface *()> CustomLazyImageLoaderCallBack_ = nullptr;
	};

	class cellblock_visitor
	{
	public:
		template <typename T>
		explicit cellblock_visitor(const T &_t) : object{&_t}
			//,
			//	getMaxVerticalCells_{ [](const void* obj) {
			//	return static_cast<const T*>(obj)->getMaxVerticalCells();
			//} },
			/*getCellSpacing_{ [](const void* obj) {
			  return static_cast<const T*>(obj)->getCellSpacing();
			} },
			getCellWidth_{ [](const void* obj) {
			  return static_cast<const T*>(obj)->getCellWidth();
			} }*/
		{
		}

	private:
		const void *object;
		void (*getMaxVerticalCells_)(const void *);
		void (*getCellSpacing_)(const float *);
		void (*getCellWidth_)(const float *);
		void (*getLowestBoundCell_)(const float *);
	};

	template <typename T>
	concept is_cellblock = requires(T _t) {
		//{_t.getMaxVerticalCells()}->std::convertible_to<int>;
		_t.getMaxVerticalCells();
		_t.getLowestBoundCell();
		_t.getCellSpacing();
		_t.getCellWidth();
	};

	//class Select : Context, IView;

	class Cell : public Context, IView
	{
	public:
		// friend class CellBlock;
		using Context::adaptiveVsync;
		using Context::event;
		using Context::getContext;
		using Context::pv;
		using IView::bounds;
		using IView::getView;
		using IView::getChildView;
		using IView::setChildView;
		// using IView::type;
		using IView::action;
		using IView::hide;
		using IView::is_form;
		using IView::isHidden;
		using IView::prevent_default_behaviour;
		using IView::show;
		using IView::child;

		using FormData = std::unordered_map<std::string, std::string>;

	public:
		// std::string type
	public:
		uint64_t index = 0;
		SDL_Color bg_color = {0x00, 0x00, 0x00, 0x00};
		SDL_Color onHoverBgColor = {0x00, 0x00, 0x00, 0x00};
		float corner_radius = 0.f;
		std::any user_data, dataSetChangedData;
		std::function<void(Cell &)> customDrawCallback;
		std::function<bool(Cell &)> customEventHandlerCallback;
		std::function<void(Cell &, std::any _data)> onDataSetChanged;
		std::function<void(Cell &, FormData _data)> onFormSubmit;
		std::deque<TextBox> textBox;
		std::deque<EditBox> editBox;
		std::deque<RunningText> runningText;
		std::deque<Expr::Slider> sliders;
		std::deque<ImageButton> imageButton;
		bool selected = false;
		bool isHighlighted = false;
		bool highlightOnHover = false;

	public:
		Cell &setContext(Context *context_) noexcept
		{
			Context::setContext(context_);
			Context::setView(this);
			return *this;
		}

		Cell &registerCustomDrawCallback(std::function<void(Cell &)> _customDrawCallback) noexcept
		{
			this->customDrawCallback = std::move(_customDrawCallback);
			return *this;
		}

		Cell &registerCustomEventHandlerCallback(std::function<bool(Cell &)> _customEventHandlerCallback) noexcept
		{
			this->customEventHandlerCallback = std::move(_customEventHandlerCallback);
			return *this;
		}

		Cell &registerOnDataSetChangedCallback(std::function<void(Cell &, std::any _data)> _onDataSetChanged) noexcept
		{
			this->onDataSetChanged = std::move(_onDataSetChanged);
			return *this;
		}

		Cell &registerOnFormSubmitCallback(std::function<void(Cell &, FormData _data)> _onFormSubmit) noexcept
		{
			this->onFormSubmit = std::move(_onFormSubmit);
			return *this;
		}

		Cell &setIndex(const uint64_t &_index) noexcept
		{
			this->index = _index;
			return *this;
		}

		Cell &addTextBox(TextBoxAttributes _TextBoxAttr) noexcept
		{
			_TextBoxAttr.rect = {bounds.x + pv->to_cust(_TextBoxAttr.rect.x, bounds.w),
								 bounds.y + pv->to_cust(_TextBoxAttr.rect.y, bounds.h),
								 pv->to_cust(_TextBoxAttr.rect.w, bounds.w),
								 pv->to_cust(_TextBoxAttr.rect.h, bounds.h)};
			textBox.emplace_back()
				.Build(this, _TextBoxAttr);

			// if (is_form and _TextBoxAttr.type="submit"){
			// textBox.back().setonsubmithandler }
			return *this;
		}

		Cell& addTextBoxFront(TextBoxAttributes _TextBoxAttr) noexcept
		{
			_TextBoxAttr.rect = { bounds.x + pv->to_cust(_TextBoxAttr.rect.x, bounds.w),
								 bounds.y + pv->to_cust(_TextBoxAttr.rect.y, bounds.h),
								 pv->to_cust(_TextBoxAttr.rect.w, bounds.w),
								 pv->to_cust(_TextBoxAttr.rect.h, bounds.h) };
			textBox.emplace_front()
				.Build(this, _TextBoxAttr);

			// if (is_form and _TextBoxAttr.type="submit"){
			// textBox.back().setonsubmithandler }
			return *this;
		}

		Cell &addTextBoxVertArray(TextBoxAttributes _TextBoxAttr, float percentageMargin, std::vector<std::string> _texts)
		{
			const auto yStep = _TextBoxAttr.rect.h + percentageMargin;
			for (auto &_txt : _texts)
			{
				_TextBoxAttr.textAttributes.text = _txt;
				addTextBox(_TextBoxAttr);
				_TextBoxAttr.rect.y += yStep;
			}
			return *this;
		}

		Cell &addTextBoxHorArray(TextBoxAttributes _TextBoxAttr, std::vector<std::string> _texts)
		{
			return *this;
		}

		Cell &addTextBoxFlexArray(TextBoxAttributes _TextBoxAttr, std::vector<std::string> _texts)
		{
			return *this;
		}

		Cell &addRunningText(RunningTextAttributes _RTextAttr)
		{
			_RTextAttr.rect = {bounds.x + pv->to_cust(_RTextAttr.rect.x, bounds.w),
							   bounds.y + pv->to_cust(_RTextAttr.rect.y, bounds.h),
							   pv->to_cust(_RTextAttr.rect.w, bounds.w),
							   pv->to_cust(_RTextAttr.rect.h, bounds.h)};
			runningText.emplace_back()
				.Build(this, _RTextAttr);
			return *this;
		}
		/*
		Cell& addSlider(SliderAttributes _Attr) {
			_RTextAttr.rect = { dest.x + pv->to_cust(_RTextAttr.rect.x, dest.w),
					dest.y + pv->to_cust(_RTextAttr.rect.y, dest.h),
					pv->to_cust(_RTextAttr.rect.w, dest.w),
					pv->to_cust(_RTextAttr.rect.h, dest.h) };
			runningText.emplace_back()
				.Build(this, _RTextAttr);
			return *this;
		}
		*/
		Cell &addEditBox(EditBoxAttributes _EditBoxAttr)
		{
			_EditBoxAttr.rect = {
				bounds.x + pv->to_cust(_EditBoxAttr.rect.x, bounds.w),
				bounds.y + pv->to_cust(_EditBoxAttr.rect.y, bounds.h),
				pv->to_cust(_EditBoxAttr.rect.w, bounds.w),
				pv->to_cust(_EditBoxAttr.rect.h, bounds.h)};
			editBox.emplace_back()
				.Build(this, _EditBoxAttr);
			return *this;
		}

		Cell& addEditBoxFront(EditBoxAttributes _EditBoxAttr)
		{
			_EditBoxAttr.rect = {
				bounds.x + pv->to_cust(_EditBoxAttr.rect.x, bounds.w),
				bounds.y + pv->to_cust(_EditBoxAttr.rect.y, bounds.h),
				pv->to_cust(_EditBoxAttr.rect.w, bounds.w),
				pv->to_cust(_EditBoxAttr.rect.h, bounds.h) };
			editBox.emplace_front()
				.Build(this, _EditBoxAttr);
			return *this;
		}

		template <is_cellblock T>
		Cell &addImageButton(T &parent_block, ImageButtonAttributes imageButtonAttributes, const PixelSystem &pixel_system = PixelSystem::PERCENTAGE)
		{
			imageButton.emplace_back().setContext(this);
			if (pixel_system == PixelSystem::PERCENTAGE)
			{
				imageButtonAttributes.rect = SDL_FRect{
					bounds.x + pv->to_cust(imageButtonAttributes.rect.x, bounds.w),
					bounds.y + pv->to_cust(imageButtonAttributes.rect.y, bounds.h),
					pv->to_cust(imageButtonAttributes.rect.w, bounds.w),
					pv->to_cust(imageButtonAttributes.rect.h, bounds.h)};

				imageButton.back().Build(imageButtonAttributes);
			}
			else
			{
				imageButton.back().Build(imageButtonAttributes);
			}
			if (imageButtonAttributes.image_load_style == IMAGE_LD_STYLE::ASYNC_PATH || imageButtonAttributes.image_load_style == IMAGE_LD_STYLE::ASYNC_CUSTOM_SURFACE_LOADER)
				parent_block.addToCellsWithAsyncImage(&imageButton.back());
			return *this;
		}

		Cell &addImageButton(ImageButtonAttributes imageButtonAttributes, const PixelSystem &pixel_system = PixelSystem::PERCENTAGE)
		{
			imageButton.emplace_back().setContext(this);
			if (pixel_system == PixelSystem::PERCENTAGE)
			{
				imageButtonAttributes.rect = SDL_FRect{
					bounds.x + pv->to_cust(imageButtonAttributes.rect.x, bounds.w),
					bounds.y + pv->to_cust(imageButtonAttributes.rect.y, bounds.h),
					pv->to_cust(imageButtonAttributes.rect.w, bounds.w),
					pv->to_cust(imageButtonAttributes.rect.h, bounds.h)};

				imageButton.back().Build(imageButtonAttributes);
			}
			else
			{
				imageButton.back().Build(imageButtonAttributes);
			}
			return *this;
		}

		Cell& addImageButtonFront(ImageButtonAttributes imageButtonAttributes, const PixelSystem& pixel_system = PixelSystem::PERCENTAGE)
		{
			imageButton.emplace_front().setContext(this);
			if (pixel_system == PixelSystem::PERCENTAGE)
			{
				imageButtonAttributes.rect = SDL_FRect{
					bounds.x + pv->to_cust(imageButtonAttributes.rect.x, bounds.w),
					bounds.y + pv->to_cust(imageButtonAttributes.rect.y, bounds.h),
					pv->to_cust(imageButtonAttributes.rect.w, bounds.w),
					pv->to_cust(imageButtonAttributes.rect.h, bounds.h) };

				imageButton.front().Build(imageButtonAttributes);
			}
			else
			{
				imageButton.front().Build(imageButtonAttributes);
			}
			return *this;
		}

		Cell &addSlider(Expr::SliderAttributes sAttr)
		{
			sAttr.rect = {
				bounds.x + pv->to_cust(sAttr.rect.x, bounds.w),
				bounds.y + pv->to_cust(sAttr.rect.y, bounds.h),
				pv->to_cust(sAttr.rect.w, bounds.w),
				pv->to_cust(sAttr.rect.h, bounds.h),
			};
			sliders.emplace_back().Build(this, sAttr);
			return *this;
		}

		void updatePosBy(float _dx, float _dy) override
		{
			bounds.x += _dx;
			bounds.y += _dy;
			for (auto &imgBtn : imageButton)
				imgBtn.updatePosBy(_dx, _dy);
			for (auto &textBx : textBox)
				textBx.updatePosBy(_dx, _dy);
			for (auto &rtext : runningText)
				rtext.updatePosBy(_dx, _dy);
		}

		void notifyDataSetChanged(std::any _dataSetChangedData)
		{
			// add a check for empty _dataSetChangedData before storing
			// or even consider passing _dataSetChangedData directly to the onDataSetChanged() callback
			dataSetChangedData = _dataSetChangedData;
			if (onDataSetChanged != nullptr)
				onDataSetChanged(*this, dataSetChangedData);
		}

		bool handleEvent() override
		{
			bool result = false;
			auto p_ = SDL_FPoint{ -1.f, -1.f};
			if (customEventHandlerCallback != nullptr)
			{
				bool temp_res = customEventHandlerCallback(*this);
				if (prevent_default_behaviour)
				{
					// reset the default behaviour to false
					prevent_default_behaviour = false;
					return temp_res;
				}
			}

			if (hidden)return result;

			if (child) {
				if (child->handleEvent()) {
					return true;
				}
			}

			if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
			{
				bounds =
					{
						DisplayInfo::Get().toUpdatedWidth(bounds.x),
						DisplayInfo::Get().toUpdatedHeight(bounds.y),
						DisplayInfo::Get().toUpdatedWidth(bounds.w),
						DisplayInfo::Get().toUpdatedHeight(bounds.h),
					};

				for (auto& imgBtn : imageButton)
					imgBtn.handleEvent();
				for (auto& textBx : textBox)
					textBx.handleEvent();
				for (auto& _editBox : editBox)
					_editBox.handleEvent();
				return false;
			} 
			if (isHidden())
				return result;

			if (event->type == SDL_EVENT_MOUSE_BUTTON_DOWN){
				p_ = SDL_FPoint{ event->button.x, event->button.y };
				if (SDL_PointInRectFloat(&p_, &bounds))result |= true;
				//std::cout << result << ", downx:" << p_.x << ", y:" << p_.y << std::endl;
			}
			else if (event->type == SDL_EVENT_MOUSE_BUTTON_UP) {
				p_ = SDL_FPoint{ event->button.x, event->button.y };
				if (SDL_PointInRectFloat(&p_, &bounds))result |= true;
				//std::cout << result << ", upx:" << p_.x << ", y:" << p_.y << std::endl;
			}
			else if (event->type == SDL_EVENT_FINGER_DOWN) {
				p_ = SDL_FPoint{ event->tfinger.x * DisplayInfo::Get().RenderW, event->tfinger.y * DisplayInfo::Get().RenderH };
				if (SDL_PointInRectFloat(&p_, &bounds))result |= true;
				//std::cout << "mouse" << ", x:" << event->tfinger.x << ", y:" << event->tfinger.y << std::endl;
			}
			else if (event->type == SDL_EVENT_FINGER_UP) {
				p_ = SDL_FPoint{ event->tfinger.x * DisplayInfo::Get().RenderW, event->tfinger.y * DisplayInfo::Get().RenderH };
				if (SDL_PointInRectFloat(&p_, &bounds))result |= true;
				//std::cout << "mouse" << ", x:" << event->tfinger.x << ", y:" << event->tfinger.y << std::endl;
			}
			else if (event->type == SDL_EVENT_FINGER_MOTION)
			{
				p_ = SDL_FPoint{ event->tfinger.x, event->tfinger.y };
				if (SDL_PointInRectFloat(&p_, &bounds))result |= true;
				//std::cout << "mouse" << ", x:" << event->tfinger.x << ", y:" << event->tfinger.y << std::endl;
			}
			else if (event->type == SDL_EVENT_MOUSE_MOTION)
			{
				p_ = SDL_FPoint{ event->motion.x, event->motion.y };
				if (SDL_PointInRectFloat(&p_, &bounds))result |= true;
				//std::cout << "mouse" << ", x:" << event->motion.x << ", y:" << event->motion.y << std::endl;
			}

			for (auto &imgBtn : imageButton)
				result |= imgBtn.handleEvent();
			for (auto &textBx : textBox)
				result |= textBx.handleEvent();
			for (auto &_editBox : editBox)
				_editBox.handleEvent();

			//std::cout << ", w:" << bounds.w << ", h:" << bounds.h << std::endl;

			return result;
		}

		void Draw() override
		{
			if (isHidden())
				return;
			// use custom draw fun if available
			// the default draw func has no ordering
			if (nullptr == customDrawCallback)
			{
				fillRoundedRectF(renderer,
								 {bounds.x, bounds.y, bounds.w, bounds.h}, corner_radius,
								 {bg_color.r, bg_color.g, bg_color.b, bg_color.a});

				for (auto &imgBtn : imageButton)
					imgBtn.Draw();
				for (auto &textBx : textBox)
					textBx.Draw();
				for (auto &_editBox : editBox)
					_editBox.Draw();
				for (auto &_rtext : runningText)
					_rtext.Draw();
				if (child)
					child->Draw();
			}
			else
			{
				customDrawCallback(*this);
			}

			if (child) {
				child->Draw();
			}
		}
	};

	struct CellBlockProps
	{
		SDL_FRect rect = {0.f, 0.f, 0.f, 0.f};
		Margin margin = {0.f, 0.f, 0.f, 0.f};
		float cornerRadius = 0.f;
		SDL_Color bgColor = {0, 0, 0, 0};
		float cellSpacingX = 0.f;
		float cellSpacingY = 0.f;
	};

	class CellBlock : public Context, private IView
	{
	public:
		using Context::event;
		using IView::bounds;
		using IView::child;
		using IView::getView;
		using IView::getChildView;
		using IView::setChildView;
		// using IView::type;
		using IView::hide;
		using IView::isHidden;
		using IView::prevent_default_behaviour;
		using IView::show;

	public:
		uint32_t prevLineCount = 0, consumedCells = 0, lineCount = 0, numPrevLineCells = 0;
		float consumedWidth = 0.f;

	public:
		const SDL_FRect &getBounds() const noexcept
		{
			return bounds;
		}

	 SDL_FRect& getBounds()
		{
			return bounds;
		}

		const float &getCellWidth() const noexcept
		{
			return this->cellWidth;
		}

		const uint32_t &getMaxVerticalCells() const noexcept
		{
			return numVerticalCells;
		}

		const float &getCellSpacing() const noexcept
		{
			return this->CellSpacing;
		}

		const Cell &getLowestBoundCell() const noexcept
		{
			return cells[bottomCell];
		}

		std::size_t size() const noexcept
		{
			return cells.size();
		}

		bool empty() const noexcept
		{
			return cells.empty();
		}

		Cell &back()
		{
			return cells.back();
		};

		Cell &front()
		{
			return cells.front();
		};

		void triggerRedrawSession()
		{
			adaptiveVsyncHD.startRedrawSession();
		}

		void stopRedrawSession()
		{
			adaptiveVsyncHD.stopRedrawSession();
		}

		void notifyDataSetChanged(std::any _data = 0)
		{
			// data_set_changed=true;
		}

		Cell &getCell(std::size_t _index)
		{
			return cells[_index];
		}

		Cell &getSelectedCell()
		{
			return cells[SELECTED_CELL];
		}

		auto getSelectedCellIndex()
		{
			return SELECTED_CELL;
		}

		CellBlock &setSelectedItem(const int64_t _selectedCell)
		{
			updateSelectedCell(_selectedCell);
			adaptiveVsyncHD.startRedrawSession();
			return *this;
		}

		void addToCellsWithAsyncImage(ImageButton *_cell)
		{
			cellsWithAsyncImages.push_back(_cell);
		}

		CellBlock &incrementYBy(const float &val) noexcept
		{
			this->bounds.y += val;
			return *this;
		}

		CellBlock &setCellBGColor(const SDL_Color &_cell_bg_color) noexcept
		{
			this->cell_bg_color = _cell_bg_color;
			return *this;
		}

		CellBlock &updateHeight(const float &height)
		{
			if (height == bounds.h)
				return *this;
			const float df = height - bounds.h;
			if (df >= 0.f)
				dy = df, ACTION_UP = true;
			else
				dy = 0.f, ACTION_DOWN = true;
			this->bounds.h = height;
			this->margin.h += df;
			if (texture.get() != nullptr)
				texture.reset();
			CacheRenderTarget rTargetCache(renderer);
			texture = CreateUniqueTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, (int)bounds.w, (int)bounds.h);
			SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
			SDL_SetRenderTarget(renderer, texture.get());
			renderClear(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
			rTargetCache.release(renderer);
			return *this;
		}

		CellBlock &setCellSpacing(const float cell_spacing) noexcept
		{
			this->CellSpacing = cell_spacing;
			return *this;
		}

		CellBlock &setOnCellClickedCallback(std::function<void(Cell &)> on_cell_clicked_callback) noexcept
		{
			onCellClickedCallback = std::move(on_cell_clicked_callback);
			return *this;
		}

		CellBlock &setOnFillNewCellData(
			std::function<void(Cell &)> _fillNewCellDataCallback) noexcept
		{
			fillNewCellDataCallback = std::move(_fillNewCellDataCallback);
			return *this;
		}

		CellBlock &updateNoMaxCells(const int &_noMaxCells) noexcept
		{
			maxCells = _noMaxCells;
			// first_load =true;
			return *this;
		}

		CellBlock &updateNoMaxCellsBy(const int &_numCells) noexcept
		{
			maxCells += _numCells;
			// first_load =true;
			return *this;
		}

		CellBlock &setEnabled(const bool &_enabled) noexcept
		{
			enabled = _enabled;
			return *this;
		}

		std::size_t getSizeOfPreAddedCells()
		{
			return sizeOfPreAddedCells;
		}

		CellBlock &clearAndReset()
		{
			update_top_and_bottom_cells();
			ANIM_ACTION_DN = false;
			ANIM_ACTION_UP = false;
			scrollAnimInterpolator.setIsAnimating(false);
			backIndex = frontIndex = 0;
			topCell = bottomCell = 0;
			lineCount = 0;
			consumedCells = 0;
			prevLineCount = 0;
			SELECTED_CELL = 0 - 1;
			PREV_SELECTED_CELL = 0 - 1;
			FIRST_LOAD = true;
			cellsWithAsyncImages.clear();
			visibleCells.clear();
			cells.clear();

			// pre added cells
			const int originalMaxCells = maxCells;
			for (auto preAddedCellSetUpCallback : preAddedCellsSetUpCallbacks)
			{
				update_top_and_bottom_cells();
				cells.emplace_back()
					.setContext(this)
					.setIndex(cells.size() - 1);
				/// FIXME: the cells adptiveVsync =&CellsSmartFrame should be a single
				/// call with the cell.setContext so that any cell::smartframehd can be init
				/// with the correct AdaptiveVsync cotext
				cells.back().adaptiveVsync = &CellsAdaptiveVsync;
				preAddedCellSetUpCallback(cells.back());
				visibleCells.push_back(&cells.back());
				++backIndex;
				update_top_and_bottom_cells();
			}

			if (maxCells > 0)
			{
				cells.emplace_back()
					.setContext(this)
					.setIndex(cells.size() - 1)
					.bg_color = cell_bg_color;
				cells.back().adaptiveVsync = &CellsAdaptiveVsync;
				cells.back().pv = this;
				fillNewCellDataCallback(cells.back());
				visibleCells.emplace_back(&cells.back());
				ACTION_UP = true;
			}
			adaptiveVsyncHD.startRedrawSession();
			update_top_and_bottom_cells();
			return *this;
		}

		void setHeaderCellVisible(bool is_visible)
		{
		}

		void setFooterCellVisible(bool is_visible)
		{
		}

		CellBlock &addHeaderCell(std::function<void(Cell &)> newCellSetUpCallback)
		{
			return *this;
		}

		CellBlock &addFooterCell(std::function<void(Cell &)> newCellSetUpCallback)
		{
			return *this;
		}

		CellBlock &addCell(std::function<void(Cell &)> newCellSetUpCallback)
		{
			if (!BuildWasCalled)
			{
				preAddedCellsSetUpCallbacks.push_back(newCellSetUpCallback);
				sizeOfPreAddedCells = preAddedCellsSetUpCallbacks.size();
			}
			else
			{
				// update_top_and_bottom_cells();
				auto &new_cell = cells.emplace_back()
									 .setContext(this)
									 .setIndex(cells.size() - 1);
				new_cell.adaptiveVsync = &CellsAdaptiveVsync;
				newCellSetUpCallback(new_cell);
				if (maxCells < cells.size())
					maxCells = cells.size();
				if (new_cell.bounds.y <= margin.h)
				{
					visibleCells.push_back(&cells.back());
					//++backIndex;
				}
				// ACTION_DOWN = true;
				SIMPLE_RE_DRAW = true;
				update_top_and_bottom_cells();
			}
			return *this;
		}

		/*
		 * instead of going through all visible cell to get the
		 * lowest and highest bound cell, calculate them on the fly and cache them as cells get added.
		 * this approach is not only faster but fixes the bug introduced by adding cells manually
		 * on a non empty block
		 */
		CellBlock &setCellRect(Cell &_cell, const uint32_t &numVerticalModules, const float &h, const float &margin_x = 0.f, const float &margin_y = 0.f)
		{
			if (getMaxVerticalCells() - consumedCells < numVerticalModules)
				lineCount++, _cell.bounds.y += margin_y;
			// if (rect.w - consumedWidth < numVerticalModules)lineCount++;
			auto lbc = getLowestBoundCell().bounds;
			if (cells.size() > 1)
			{
				const auto &bck = cells[cells.size() - 2].bounds;
				if (lbc.y + lbc.h < bck.y + bck.h)
					lbc = bck;
			}

			_cell.bounds.y += lbc.y;
			if (lineCount != prevLineCount)
				consumedCells = 0, _cell.bounds.y += lbc.h + CellSpacingY;
			_cell.bounds.x = (consumedCells * (getCellWidth() + CellSpacingX)) + margin_x;
			_cell.bounds.w = static_cast<float>(numVerticalModules) * (getCellWidth()) + (numVerticalModules);
			_cell.bounds.w -= margin_x * 2.f;
			_cell.bounds.h = h - (margin_y * 2.f);
			prevLineCount = lineCount;
			consumedCells += numVerticalModules;
			// consumedWidth += static_cast<float>(numVerticalModules) * (getCellWidth());
			return *this;
		}

		void resetTexture()
		{
			CacheRenderTarget crt_(renderer);
			texture.reset();
			texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
										  SDL_TEXTUREACCESS_TARGET, (int)margin.w, (int)margin.h);
			SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
			SDL_SetRenderTarget(renderer, texture.get());
			renderClear(renderer, 0, 0, 0, 0);
			// fillRoundedRectF(renderer, { 0.f,0.f,rect.w,rect.h }, cornerRadius, bgColor);
			crt_.release(renderer);
		}

		CellBlock &Build(Context *context_, const int &maxCells_, const int &NumVerticalModules, const CellBlockProps &_blockProps, const ScrollDirection &scroll_direction = ScrollDirection::VERTICAL)
		{
			Context::setContext(context_);
			Context::setView(this);
			adaptiveVsyncHD.setAdaptiveVsync(adaptiveVsync);
			pv = this;
			bounds = _blockProps.rect;
			margin = {
				bounds.x + DisplayInfo::Get().to_cust(_blockProps.margin.left, bounds.w),
				bounds.y + DisplayInfo::Get().to_cust(_blockProps.margin.top, bounds.h),
				DisplayInfo::Get().to_cust(100.f - (_blockProps.margin.left + _blockProps.margin.right), bounds.w),
				DisplayInfo::Get().to_cust(100.f - (_blockProps.margin.top + _blockProps.margin.bottom), bounds.h)};
			bgColor = _blockProps.bgColor;
			cornerRadius = _blockProps.cornerRadius;
			maxCells = maxCells_;
			numVerticalCells = NumVerticalModules;
			CellSpacingX = _blockProps.cellSpacingX;
			CellSpacingY = _blockProps.cellSpacingY;
			cellWidth = ((margin.w - (_blockProps.cellSpacingX * (NumVerticalModules - 1))) / static_cast<float>(NumVerticalModules));
			if (texture.get() != nullptr)
				texture.reset();
			CacheRenderTarget crt_(renderer);
			texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
										  SDL_TEXTUREACCESS_TARGET, (int)margin.w, (int)margin.h);
			SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
			SDL_SetRenderTarget(renderer, texture.get());
			renderClear(renderer, 0, 0, 0, 0);
			// fillRoundedRectF(renderer, { 0.f,0.f,rect.w,rect.h }, cornerRadius, bgColor);
			crt_.release(renderer);
			const int originalMaxCells = maxCells;
			for (auto preAddedCellSetUpCallback : preAddedCellsSetUpCallbacks)
			{
				update_top_and_bottom_cells();
				cells.emplace_back()
					.setContext(this)
					.setIndex(cells.size() - 1)
					.adaptiveVsync = adaptiveVsync;
				preAddedCellSetUpCallback(cells.back());
				maxCells++;
				visibleCells.push_back(&cells.back());
				++backIndex;
				update_top_and_bottom_cells();
			}

			// preAddedCellsSetUpCallbacks.clear();

			if (originalMaxCells > 0)
			{
				cells.emplace_back()
					.setContext(this)
					.setIndex(cells.size() - 1)
					.bg_color = cell_bg_color;
				cells.back().pv = this;
				cells.back().adaptiveVsync = adaptiveVsync;
				fillNewCellDataCallback(cells.back());
				visibleCells.emplace_back(&cells.back());
				ACTION_UP = true;
			}
			else
			{
				SDL_Log("empty layout");
			}
			BuildWasCalled = true;
			// buildFrameHD.startRedrawSession();
			return *this;
		}

		[[nodiscard]] bool isPosInbound(const float &valX, const float &valY) const noexcept
		{
			if (valX > bounds.x && valX < bounds.x + bounds.w && valY > bounds.y &&
				valY < bounds.y + bounds.h)
				return true;
			return false;
		}

		[[nodiscard]] bool isBlockScrollable() const
		{
			if (cells.empty())
				return false;
			if (cells.back().bounds.y + cells.back().bounds.h > margin.h && cells.front().bounds.y < 0.f)
				return true;
			return false;
		}

		bool handleEvent() override
		{
			bool result = false;
			if (not enabled or hidden)
				return result;
			if (child) {
				if (child->handleEvent()) {
					updateHighlightedCell(-1);
					SIMPLE_RE_DRAW = true;
					return true;
				}
			}
			if (event->type == SDL_EVENT_RENDER_TARGETS_RESET)
			{
				// SDL_Log("targets reset. must recreate texts textures");
				allCellsHandleEvent();
				SIMPLE_RE_DRAW = true;
			}
			else if (event->type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED)
			{
				bounds =
					{
						DisplayInfo::Get().toUpdatedWidth(bounds.x),
						DisplayInfo::Get().toUpdatedHeight(bounds.y),
						DisplayInfo::Get().toUpdatedWidth(bounds.w),
						DisplayInfo::Get().toUpdatedHeight(bounds.h),
					};
				margin =
					{
						DisplayInfo::Get().toUpdatedWidth(margin.x),
						DisplayInfo::Get().toUpdatedHeight(margin.y),
						DisplayInfo::Get().toUpdatedWidth(margin.w),
						DisplayInfo::Get().toUpdatedHeight(margin.h),
					};
				cellWidth = margin.w / static_cast<float>(numVerticalCells);
				resetTexture();
				// clearAndReset();
				allCellsHandleEvent();
				SIMPLE_RE_DRAW = true;
			}
			else if (event->type == SDL_EVENT_FINGER_DOWN)
			{
				pf = cf = {event->tfinger.x * DisplayInfo::Get().RenderW,
						   event->tfinger.y * DisplayInfo::Get().RenderH};
				if (isPosInbound(cf.x, cf.y))
				{
					if (child and not child->hidden)
					{
						child->hide();
						return true;
					}
					SIMPLE_RE_DRAW = true;
					KEYDOWN = true, scrollAnimInterpolator.setIsAnimating(false);
					visibleCellsHandleEvent();
					result or_eq true;
				}
				FingerMotion_TM = SDL_GetTicks();
			}
			else if (event->type == SDL_EVENT_MOUSE_WHEEL)
			{
				// SDL_Log("mswm: %f", event.wheel.preciseY);
				KEYDOWN = true;
				MOTION_OCCURED = true;
				if (KEYDOWN and maxCells > 0)
				{
					// cf = { (event.wheel.preciseX), (event.wheel.preciseY) };
					// moved_distance += {event.tfinger.dx* DisplayInfo::Get().RenderW, event.tfinger.dy* DisplayInfo::Get().RenderH};
					internal_handle_motion();
				}
				FingerUP_TM = SDL_GetTicks();
			}
			else if (event->type == SDL_EVENT_FINGER_MOTION)
			{
				MOTION_OCCURED = true;
				visibleCellsHandleEvent();
				if (KEYDOWN and maxCells > 0)
				{
					cf = {event->tfinger.x * DisplayInfo::Get().RenderW,
						  event->tfinger.y * DisplayInfo::Get().RenderH};
					movedDistance += {event->tfinger.dx * DisplayInfo::Get().RenderW, event->tfinger.dy * DisplayInfo::Get().RenderH};
					internal_handle_motion();
					updateHighlightedCell(-1);
				}
				FingerUP_TM = SDL_GetTicks();
			}
			else if (event->type == SDL_EVENT_MOUSE_MOTION)
			{
				if (not KEYDOWN)
				{
					// current finger
					cf = {(float)event->motion.x, (float)event->motion.y};
					// current finger transformed
					const v2d_generic<float> cf_trans = {cf.x - margin.x, cf.y - margin.y};
					bool cellFound = false;
					if (isPosInbound(event->motion.x, event->motion.y))
					{
						result or_eq true;
						auto fc = std::find_if(visibleCells.begin(), visibleCells.end(),
											   [this, &cf_trans, &cellFound](Cell *cell)
											   {
												   if (cf_trans.y >= cell->bounds.y and
													   cf_trans.y <= cell->bounds.y + cell->bounds.h and cf_trans.x >= cell->bounds.x and
													   cf_trans.x <= cell->bounds.x + cell->bounds.w)
												   {
													   cell->handleEvent();
													   updateHighlightedCell(cell->index);
													   SIMPLE_RE_DRAW = true;
													   cellFound = true;
													   return true;
												   }
												   return false;
											   });
						if (not cellFound)
						{
							if (HIGHLIGHTED_CELL >= 0)
								SIMPLE_RE_DRAW = true;
							updateHighlightedCell(-1);
						}
					}
					else
					{
						if (HIGHLIGHTED_CELL >= 0)
							SIMPLE_RE_DRAW = true;
						updateHighlightedCell(-1);
					}
				}
			}
			else if (event->type == SDL_EVENT_FINGER_UP)
			{
				const auto finger_up_dt = (SDL_GetTicks() - FingerUP_TM);
				scrollAnimInterpolator.setIsAnimating(false);
				if (KEYDOWN and not MOTION_OCCURED)
				{
					cf = {event->tfinger.x * DisplayInfo::Get().RenderW,
						  event->tfinger.y * DisplayInfo::Get().RenderH};
					const v2d_generic<float> cf_trans = {cf.x - margin.x, cf.y - margin.y};
					if (isPosInbound(cf.x, cf.y))
					{
						result or_eq true;
						auto fc = std::find_if(visibleCells.begin(), visibleCells.end(),
											   [this, &cf_trans](Cell *cell)
											   {
												   if (cf_trans.y >= cell->bounds.y and
													   cf_trans.y <= cell->bounds.y + cell->bounds.h and cf_trans.x >= cell->bounds.x and
													   cf_trans.x <= cell->bounds.x + cell->bounds.w)
												   {
													   updateSelectedCell(cell->index);
													   cell->handleEvent();
													   CELL_PRESSED = true;
													   pressed_cell_index = cell->index;
													   updateHighlightedCell(-1);
													   return true;
												   }
												   return false;
											   });
					}
				}

				if (MOTION_OCCURED and isPosInbound(cf.x, cf.y) and isBlockScrollable())
				{
					result or_eq true;
					// SDL_Log("FU_TM: %d", finger_up_dt);
					float dt = static_cast<float>(SDL_GetTicks() - FingerMotion_TM);
					if (finger_up_dt > 45)
						dt = 5000.f, movedDistance.y = 0.1f;
					if (dt == 0.f)
						dt = 0.0001f;
					auto speed = (SDL_fabsf(movedDistance.y) / dt) * 10.f;
					// SDL_Log("Speed: %f", speed);
					if (finger_up_dt > 45)
						speed /= 10.f;
					// SDL_Log("DS_MOVED: %f REL_TM: %d INIT_VEL: %f", fabs(moved_distance.y), dt, speed);
					if (movedDistance.y >= 0.f)
						ANIM_ACTION_DN = true;
					else
						ANIM_ACTION_UP = true;
					scrollAnimInterpolator.start(speed);
					movedDistance = {0.f, 0.f};
				}

				KEYDOWN = false;
				MOTION_OCCURED = false;
			}
			/*else if (event->type == SDL_WINDOWEVENT) {
				//if (event.window.type == SDL_WINDOWEVENT) {
				//	//SDL_GetWindowPosition(window, NULL, NULL);
				//	//SDL_Log("WARN: WINDOW_EVENT");
				//}
			}*/
			else
			{
				visibleCellsHandleEvent();
			}

			if (CELL_PRESSED or SIMPLE_RE_DRAW or scrollAnimInterpolator.isAnimating() or CellsAdaptiveVsync.hasRequests())
			{
				adaptiveVsyncHD.startRedrawSession();
			}
			return result; // *this;
		}

		void scroll(float distance)
		{
			if (distance >= 0.f)
				ANIM_ACTION_DN = true;
			else
				ANIM_ACTION_UP = true;
			SIMPLE_RE_DRAW = false;
			scrollAnimInterpolator.startWithDistance(distance);
		}

		// BUG: infinite recursion if you invoke this func inside a cell pressed callback
		void Draw() override
		{
			if (not enabled or isHidden())
				return;
			update_cells_with_async_images();
			if (SIMPLE_RE_DRAW)
			{
				SIMPLE_RE_DRAW = false;
				goto simple_re_draw;
			}
			if (not ACTION_DOWN and not ACTION_UP and not ANIM_ACTION_DN and not ANIM_ACTION_UP and not CELL_PRESSED and not adaptiveVsyncHD.shouldReDrawFrame())
			{
				fillRoundedRectF(renderer, {bounds.x, bounds.y, bounds.w, bounds.h}, cornerRadius, bgColor);
				SDL_RenderTexture(renderer, texture.get(), nullptr, &margin);
				if (child) {
					child->Draw();
				}
				adaptiveVsyncHD.stopRedrawSession();
				// goto simple_re_draw;
				return;
			}

			if (scrollAnimInterpolator.isAnimating())
			{
				scrollAnimInterpolator.update();
				if (ANIM_ACTION_DN)
				{
					dy = scrollAnimInterpolator.getValue(), ACTION_DOWN = true;
					if (cells.front().bounds.y + dy > 0.f /*&& cells.front().index <= 0*/)
						dy = 0.f - (cells.front().bounds.y), scrollAnimInterpolator.setIsAnimating(false), ACTION_UP = false, ACTION_DOWN = false;
				}
				else if (ANIM_ACTION_UP)
				{
					dy = 0.f - scrollAnimInterpolator.getValue(), ACTION_UP = true;
					if (cells.back().bounds.y + cells.back().bounds.h + dy < margin.h and
						cells.back().index >= (maxCells - 1))
						dy = margin.h - cells.back().bounds.y -
							 cells.back().bounds.h,
						scrollAnimInterpolator.setIsAnimating(false), ACTION_UP = false, ACTION_DOWN = false;
				}
			}
			else
				ANIM_ACTION_DN = ANIM_ACTION_UP = false;

			if (not cells.empty())
			{
				if (dy not_eq 0.f)
				{
					movedDistanceSinceStart.y += dy;
					// std::for_each(cells.begin(), cells.end(), [this](Cell<CustomCellData>& cell) {Async::GThreadPool.enqueue(&Cell<CustomCellData>::updatePosBy, &cell,0.f, dy); });
					std::for_each(cells.begin(), cells.end(), [this](Cell &cell)
								  { cell.updatePosBy(0.f, dy); });
				}
				if (CELL_PRESSED)
				{
					CELL_PRESSED = false;
					if (onCellClickedCallback)
						onCellClickedCallback(cells[pressed_cell_index]);
					pressed_cell_index = 0;
				}

				if (cells.back().index >= maxCells - 1)
					FIRST_LOAD = false;

				if (ACTION_UP)
				{
					// auto beg = std::chrono::steady_clock::now();
					while (visibleCells.back()->bounds.y < margin.h and
						   visibleCells.back()->index < (maxCells - 1))
					{
						update_top_and_bottom_cells();
						if (FIRST_LOAD and (cells.back().index == visibleCells.back()->index) and cells.back().index < (maxCells - 1))
						{
							cells.emplace_back();
							cells.back().setContext(this).setIndex(cells[cells.size() - 2].index + 1).bg_color = cell_bg_color;
							cells.back().adaptiveVsync = &CellsAdaptiveVsync;
							cells.back().pv = this;
							fillNewCellDataCallback(cells.back());
						}
						++backIndex;
						visibleCells.push_back(&cells[backIndex]);
					}
					while (visibleCells[0]->bounds.y + visibleCells[0]->bounds.h < 0.f)
						visibleCells.pop_front(), frontIndex = visibleCells.front()->index;
				}

				if (ACTION_DOWN)
				{
					// auto beg = std::chrono::steady_clock::now();
					while (visibleCells[0]->index > cells.front().index and visibleCells[0]->bounds.y + visibleCells[0]->bounds.h > 0.f)
					{
						--frontIndex;
						visibleCells.push_front(&cells[frontIndex]);
						update_top_and_bottom_cells();
					}
					while (visibleCells.back()->bounds.y > margin.h)
						visibleCells.pop_back(), backIndex = visibleCells.back()->index, update_top_and_bottom_cells(); /*, SDL_Log("back pop rs: %d", cells_tmp.size())*/
				}

				dy = 0.f;
				ACTION_DOWN = ACTION_UP = false;
			}

		simple_re_draw:
			fillRoundedRectF(renderer, {bounds.x, bounds.y, bounds.w, bounds.h}, cornerRadius, bgColor);
			CacheRenderTarget crt_(renderer);
			SDL_SetRenderTarget(renderer, texture.get());
			renderClear(renderer, 0, 0, 0, 0);
			for (auto &cell : visibleCells)
			{
				cell->Draw();
			}
			crt_.release(renderer);
			SDL_RenderTexture(renderer, texture.get(), nullptr, &margin);
			if (child) {
				child->Draw();
			}
			adaptiveVsyncHD.stopRedrawSession();
			if (CellsAdaptiveVsync.hasRequests() or scrollAnimInterpolator.isAnimating())
			{
				adaptiveVsyncHD.startRedrawSession();
			}
		}

	protected:
		void update_cells_with_async_images()
		{
			if (cellsWithAsyncImages.empty())
				return;
			std::erase_if(cellsWithAsyncImages, [&](ImageButton *_imgBtn)
						  {
					if (_imgBtn->isAsyncLoadComplete()) { _imgBtn->Draw(); return true; }
					return false; });
		}

		void update_top_and_bottom_cells() noexcept
		{
			float smallest = 0.f, tmp_smallest = 0.f;
			std::size_t tmp_top_cell = 0;
			float largest = 0.f, tmp_largest = 0.f;
			std::size_t tmp_bottom_cell = 0;
			std::for_each(visibleCells.begin(), visibleCells.end(),
						  [&smallest, &tmp_smallest, &tmp_top_cell, &largest, &tmp_largest, &tmp_bottom_cell](const Cell *cell)
						  {
							  tmp_smallest = cell->bounds.y;
							  tmp_largest = cell->bounds.y + cell->bounds.h;
							  if (tmp_smallest < smallest)
								  smallest = tmp_smallest, tmp_top_cell = cell->index;
							  if (tmp_largest >= largest)
								  largest = tmp_largest, tmp_bottom_cell = cell->index;
							  tmp_smallest = 0.f;
							  tmp_largest = 0.f;
						  });
			topCell = tmp_top_cell;
			bottomCell = tmp_bottom_cell;
			// SDL_Log("NBT_CELL: %d", bottomCell);
			// SDL_Log("NTP_CELL: %d", topCell);
		}

		void updateSelectedCell(int64_t _selectedCell)
		{
			PREV_SELECTED_CELL = SELECTED_CELL;
			SELECTED_CELL = _selectedCell;
			if (PREV_SELECTED_CELL >= 0)
				cells[PREV_SELECTED_CELL].selected = false;
			cells[SELECTED_CELL].selected = true;
		}

		void updateHighlightedCell(int64_t _highlightedCell)
		{
			// clear the prev highlighted cell
			if (HIGHLIGHTED_CELL >= 0)
				cells[HIGHLIGHTED_CELL].isHighlighted = false;
			// PREV_HIGHLIGHTED_CELL = HIGHLIGHTED_CELL;
			HIGHLIGHTED_CELL = _highlightedCell;
			// if (PREV_HIGHLIGHTED_CELL >= 0)cells[PREV_HIGHLIGHTED_CELL].isHighlighted = false;
			if (HIGHLIGHTED_CELL >= 0)
				cells[HIGHLIGHTED_CELL].isHighlighted = true;
		}

		bool visibleCellsHandleEvent()
		{
			bool result = false;
			for (auto &_cell : visibleCells)
			{
				_cell->handleEvent();
			}
			return result;
		}

		bool allCellsHandleEvent()
		{
			bool result = false;
			for (auto &_cell : cells)
			{
				_cell.handleEvent();
			}
			return result;
		}

	private:
		void internal_handle_motion()
		{
			// ACTION_UP
			if (cf.y < pf.y)
			{
				if (cells.back().bounds.y + cells.back().bounds.h - (pf.y - cf.y) <
						margin.h &&
					cells.back().index == maxCells - 1)
				{
					dy += 0.f;
				}
				else if (cf.y > margin.y + margin.h || cf.y < margin.y)
				{
					dy += 0.f;
				}
				else
				{
					dy -= (pf.y - cf.y);
					if (cells.back().bounds.y + cells.back().bounds.h + dy < margin.h &&
						cells.back().index >= (maxCells - 1))
						dy = margin.h - cells.back().bounds.y - cells.back().bounds.h;
					// SDL_Log("DY: %f", dy);
					ACTION_UP = true, ACTION_DOWN = false;
				}
			}
			// ACTION_DOWN
			else if (pf.y < cf.y)
			{
				if ((cells[0].bounds.y + (cf.y - pf.y) > 0.f && cells[0].index < 1) ||
					(cf.y > margin.y + margin.h) || cf.y < margin.y)
				{
					dy += 0.f;
				}
				// else if (cfy > bottomBar.dest.y) { dy += 0; }
				else
				{
					// scrollBar.set_then(SDL_GetTicks());
					dy += (cf.y - pf.y);
					if (cells.front().bounds.y + dy > 0 && cells.front().index <= 0)
						dy = SDL_fabsf(cells.front().bounds.y);
					ACTION_DOWN = true, ACTION_UP = false;
				}
			}
			// dy *= 2.f;
			pf = cf;
		}

	private:
		std::function<void(Cell &)> onCellClickedCallback = nullptr;
		std::function<void(Cell &)> fillNewCellDataCallback = nullptr;
		std::deque<Cell> cells;
		std::deque<Cell *> visibleCells;
		std::deque<ImageButton *> cellsWithAsyncImages;
		std::deque<std::function<void(Cell &)>> preAddedCellsSetUpCallbacks;
		Cell header_cell, footer_cell;
		AdaptiveVsyncHandler adaptiveVsyncHD;
		AdaptiveVsync CellsAdaptiveVsync;
		SDL_FRect margin;
		SharedTexture texture;
		SDL_Color cell_bg_color = {0x00, 0x00, 0x00, 0x00};
		uint64_t maxCells = 0;
		// SDL_FRect dest;
		float cellWidth = 0.f, CellSpacing = 0.f, CellSpacingX = 0.f, CellSpacingY = 0.f;
		float dy = 0.f;
		float cornerRadius = 0.f;
		/*current finger*/
		v2d_generic<float> cf = {0.f, 0.f};
		/*previous finger*/
		v2d_generic<float> pf = {0.f, 0.f};
		v2d_generic<float> movedDistance = {0.f, 0.f};
		v2d_generic<float> movedDistanceSinceStart = {0.f, 0.f};
		uint32_t numVerticalCells = 0;
		Uint32 FingerMotion_TM = 0;
		Uint32 FingerUP_TM = 0;
		SDL_Color bgColor = {0, 0, 0, 0xff};
		std::size_t backIndex = 0, frontIndex = 0;
		std::size_t topCell = 0, bottomCell = 0;
		bool enabled = true, ACTION_UP = false, ACTION_DOWN = false, CELL_PRESSED = false, CELL_HIGHLIGHTED = false, KEYDOWN = false;
		bool MOTION_OCCURED = false, ANIM_ACTION_UP = false, ANIM_ACTION_DN = false, FIRST_LOAD = true;
		bool SIMPLE_RE_DRAW = false;
		bool BuildWasCalled = false;
		int64_t SELECTED_CELL = 0 - 1, PREV_SELECTED_CELL = 0 - 1;
		int64_t HIGHLIGHTED_CELL = 0 - 1, PREV_HIGHLIGHTED_CELL = 0 - 1;
		std::size_t sizeOfPreAddedCells = 0;
		std::size_t pressed_cell_index = 0;
		Interpolator scrollAnimInterpolator;
	};

	struct SelectProps {
		SDL_FRect rect{ 0.f,0.f,0.f,0.f };
		SDL_FRect inner_block_rect{ 0.f,0.f,0.f,0.f };
		SDL_FRect text_margin{ 0.f,0.f,0.f,0.f };
		SDL_Color text_color{ 0x00,0x00,0x00,0xff };
		SDL_Color bg_color{ 0xff,0xff,0xff,0xff };
		SDL_Color on_hover_color{};
		float border_size = 1.f;
		float corner_radius = 1.f;
		int maxValues = 1;
	};


	class Select : Context {
	public:
		struct Value {
			std::size_t id = 0;
			std::string value = "";
		};
	public:
		CellBlock* getInnerBlock() { return &valuesBlock; }
		Cell* getInnerCell() { return &viewValue; }

		IView* getView()
		{
			return viewValue.getView();
		}

		IView* show()
		{
			return viewValue.show();
		}

		IView* hide()
		{
			return viewValue.hide();
		}
	public:
		Select& Build(Context* _context, SelectProps _props, std::vector<Value> _values, std::size_t _default=0) {
			Context::setContext(_context);
			const auto bounds = _props.rect;
			_props.inner_block_rect.x = bounds.x;
			_props.inner_block_rect.y = bounds.y + bounds.h + 2.f;
			values_ = _values;
			std::size_t _max_values = _values.size();
			//if all values rect dimensions arent set we defaut
			if (_props.inner_block_rect.w <= 0.f or _props.inner_block_rect.w <= 0.f) {
				float tmpH = _max_values >= 4 ? 4.f : static_cast<float>(_max_values);
				_props.inner_block_rect = { bounds.x, bounds.y + bounds.h + 1.f, bounds.w, bounds.h * tmpH };
			}

			viewValue.setContext(_context);
			viewValue.bg_color = _props.bg_color;
			viewValue.corner_radius = _props.corner_radius;
			viewValue.bounds = bounds;
			if (_values.size()) {
				viewValue.addTextBox(
					{
						//.mem_font = Font::OpenSansSemiBold,
						.rect = {5.f,5.f,90.f,90.f},
						.textAttributes = { values_[_default].value, {0,0,0,0xff}, {0, 0, 0, 0}},
						.gravity = Gravity::LEFT
					}
				);
			}

			viewValue.registerCustomEventHandlerCallback([this](Cell& _cell)->bool {
				if (viewValue.child->handleEvent()) {
					return true;
				}
				if (event->type == SDL_EVENT_FINGER_DOWN){
					//mouse point
					auto mp = SDL_FPoint{ event->tfinger.x * DisplayInfo::Get().RenderW,
							   event->tfinger.y * DisplayInfo::Get().RenderH };
					if (SDL_PointInRectFloat(&mp, &_cell.bounds)/* or p2*/) {
						viewValue.child->toggleView();
						return true;
					}
					else
					{
						if (not valuesBlock.isHidden()) {
							valuesBlock.hide();
							return true;
						}
						valuesBlock.hide();
					}
				}
				return false;
				});

			valuesBlock.setOnFillNewCellData(
				[this, bounds](Cell& _cell) {
					valuesBlock.setCellRect(_cell, 1, bounds.h, 0.f, 1.f);
					_cell.bg_color = viewValue.bg_color;
					_cell.onHoverBgColor = { 180,180,180,0xff };
					_cell.highlightOnHover = true;
					_cell.addTextBox(
						{
							//.mem_font = Font::OpenSansSemiBold,
							.rect = {5.f,5.f,90.f,90.f},
							.textAttributes = { values_[_cell.index].value, {0,0,0,0xff}, {0, 0, 0, 0}},
							.gravity = Gravity::LEFT
						}
					);

					_cell.textBox[0].addOnclickedCallback([this, &_cell](TextBox* _textbox) {
						viewValue.textBox.pop_back();
						selectedVal = _cell.index;
						viewValue.addTextBox(
							{
								//.mem_font = Font::OpenSansSemiBold,
								.rect = {5.f,5.f,90.f,90.f},
								.textAttributes = { values_[selectedVal].value, {0,0,0,0xff}, {0, 0, 0, 0} },
								.gravity = Gravity::LEFT
							}
						);
						valuesBlock.hide();
						});
				});
			valuesBlock.Build(_context, _max_values, 1,
				{
				.rect = _props.inner_block_rect,
				.cornerRadius = _props.corner_radius,
				.bgColor = _props.bg_color
				}
			);
			viewValue.setChildView(valuesBlock.getView()->hide());

			return *this;
		}

		Select& Build(Context* _context, SelectProps _props, int _max_values, const int& _numVerticalGrid, std::function<void(Cell&)> _valueCellOnCreateCallback) {
			Context::setContext(_context);
			auto bounds = _props.rect;
			//if all values rect dimensions arent set we defaut
			if (_props.inner_block_rect.w <= 0.f or _props.inner_block_rect.w) {
				float tmpH = _max_values >= 4 ? 4.f : static_cast<float>(_max_values);
				_props.inner_block_rect = { bounds.x, bounds.y + bounds.h, bounds.w, bounds.h * tmpH };
			}

			viewValue.setContext(_context);
			viewValue.bg_color = _props.bg_color;
			viewValue.corner_radius = _props.corner_radius;
			viewValue.bounds = bounds;

			valuesBlock.setOnFillNewCellData(_valueCellOnCreateCallback);
			valuesBlock.Build(_context, _max_values, _numVerticalGrid,
				{
				.rect = _props.inner_block_rect,
				.bgColor = _props.bg_color
				}
			);

			return *this;
		}
		
		//void addValueFront(Value newVal) { }

		void addValue(Value newVal) {
			if (values_.empty()) {
				viewValue.addTextBox(
					{
						//.mem_font = Font::OpenSansSemiBold,
						.rect = {5.f,5.f,90.f,90.f},
						.textAttributes = { newVal.value, {0,0,0,0xff}, {0, 0, 0, 0}},
						.gravity = Gravity::LEFT
					}
				);
			}
			values_.emplace_back(newVal);
			valuesBlock.updateNoMaxCells(values_.size());
			valuesBlock.clearAndReset();
			//viewValue.setChildView(valuesBlock.getView()->hide());
		}
		

		Value& getSelectedValue() {
			return values_[selectedVal];
		}

		std::vector<Value> getAllValues() { return values_; }

		bool hasValues() { return not values_.empty(); }

		void reset() { values_ = {}; }

		bool handleEvent() {
			switch (event->type) {

			}
			return viewValue.handleEvent();
		}

		void Draw() {
			viewValue.Draw();
		}

	private:
		Cell viewValue;
		CellBlock valuesBlock;
		std::vector<Value> values_;
		std::size_t selectedVal = 0;
	};



	using MenuProps = CellBlockProps;

	class Menu : Context, IView
	{
	public:
		using IView::getView;
		using IView::hide;
		using IView::isHidden;
		using IView::show;

	public:
		Menu &Build(Context *_context, const int &NumVerticalModules, const MenuProps &_menuProps)
		{
			setContext(_context);
			pv = this;
			bounds = _menuProps.rect;

			menu_block.Build(_context, 0, NumVerticalModules, _menuProps);

			build_successful = true;
			return *this;
		}

		Menu &addItem(std::function<void(Cell &)> newCellSetUpCallback)
		{
			menu_block.addCell(std::move(newCellSetUpCallback));
			return *this;
		}

		Menu &setCellRect(Cell &_cell, const uint32_t &numVerticalModules, const float &h)
		{
			menu_block.setCellRect(_cell, numVerticalModules, h);
			return *this;
		}

		Menu &setSelectedItem(const int64_t &selectedItem)
		{
			menu_block.setSelectedItem(selectedItem);
			return *this;
		}

		Menu &registerOnClickedCallback(std::function<void(Cell &)> on_cell_clicked_callback)
		{
			menu_block.setOnCellClickedCallback(std::move(on_cell_clicked_callback));
			return *this;
		}

		bool handleEvent() override
		{
			return menu_block.handleEvent();
		}

		void Draw() override
		{
			menu_block.Draw();
		}

		Menu &setCornerRadius(const float &cornerRad)
		{
			// menu_block.setCornerRadius(cornerRad);
			return *this;
		}

		CellBlock &getCellBlock()
		{
			return menu_block;
		}

	private:
		CellBlock menu_block;
		bool build_successful = false;
	};

} // namespace MYLIB

// #endif //VOLT_H
