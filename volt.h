#include <stdio.h>
#ifdef _MSC_VER
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#else
#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>
#endif

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
// #include <arm_neon.h> // For NEON intrinsics

#include "volt_util.h"
#include "volt_fonts.h"
//#include "mp.h"
#include "interpolators.h"
#include "interpolated.hpp"
//#include "text_editor.hpp"

static Logger GLogger;
//	helper function to convert SDL_FRect* to SDL_Rect* for compatibility with old sdl versions
// use std::launder instead
SDL_Rect *cvt_frect_to_rect(/*void*/ SDL_FRect *frect)
{
	if (frect == nullptr)
		return nullptr;

	SDL_Rect *rect1 = reinterpret_cast<SDL_Rect *>(frect);
	SDL_Rect *rect = std::launder(rect1);
	return rect;
}

SDL_Point *cvt_fpoint_to_point(SDL_FPoint *fpoint)
{
	if (fpoint == nullptr)
		return nullptr;
	SDL_Point *point1 = reinterpret_cast<SDL_Point *>(fpoint);
	SDL_Point *point = std::launder(point1);

	return point;
}
template <typename A, typename AA>
SDL_bool SDL_PointInRect2(const A *p, const AA *r)
{
	return ((p->x >= r->x) && (p->x < (r->x + r->w)) &&
			(p->y >= r->y) && (p->y < (r->y + r->h)))
			   ? SDL_TRUE
			   : SDL_FALSE;
}
#define HIGH_PIXEL_DENSITY SDL_WINDOW_ALLOW_HIGHDPI
#define EVT_WPSC SDL_WINDOWEVENT_SIZE_CHANGED
#define EVT_QUIT SDL_QUIT
#define EVT_WMAX SDL_WINDOW_MAXIMIZED
#define EVT_RENDER_TARGETS_RESET SDL_RENDER_TARGETS_RESET
#define EVT_FINGER_DOWN SDL_FINGERDOWN
#define EVT_FINGER_MOTION SDL_FINGERMOTION
#define EVT_FINGER_UP SDL_FINGERUP
#define EVT_MOUSE_BTN_DOWN SDL_MOUSEBUTTONDOWN
#define EVT_MOUSE_MOTION SDL_MOUSEMOTION
#define EVT_MOUSE_BTN_UP SDL_MOUSEBUTTONUP
//#define CreateWindow(TITLE, ...) SDL_CreateWindow(TITLE, 0 __VA_OPT__(, ) 0 __VA_OPT__(, ) __VA_ARGS__)
#define RenderPoint SDL_RenderDrawPointF
#define RenderPoints SDL_RenderDrawPointsF
#define RenderLine SDL_RenderDrawLine
#define SDL_DestroySurface SDL_FreeSurface
#define RenderTexture(rend, tex, src, dst) SDL_RenderCopyF(rend, tex, cvt_frect_to_rect(src), dst)
#define RenderFillRect(rend, dst) SDL_RenderFillRect(rend, cvt_frect_to_rect(dst))
#define RenderFillRectsF SDL_RenderFillRectsF
#define SDL_PointInRectFloat(p, r) SDL_PointInRect2(p, r)

// Using SDL_EventFilter for SDL2. For SDL3, the return type is bool.
static int last_motion_event_time = 0;
// static auto last_event_time = 0;

#if SDL_VERSION_ATLEAST(3, 0, 0)
static bool SDLCALL myEventFilter(void *userdata, SDL_Event *event)
#else
static int SDLCALL myEventFilter(void *userdata, SDL_Event *event)
#endif
{
	// static int motion_event_count = 0;
	static auto last_event_time = SDL_GetTicks();

	if (event->type == SDL_FINGERMOTION)
	{
		// last_motion_event_time = SDL_GetTicks();
		//  Rate limit based on time
		//  This is generally a better approach for smoother handling.
		//  For example, only process finger motion events every 10ms.
		auto current_time = SDL_GetTicks();
		auto duration = (current_time - last_event_time);

		const int MIN_TIME_BETWEEN_EVENTS_MS = 30; // Adjust this value as needed

		if (duration < MIN_TIME_BETWEEN_EVENTS_MS)
		{
			// std::cout << "Skipping event:"<<event->type<<" (rate limit, time: " << duration<< "ms)" << std::endl;
			return
#if SDL_VERSION_ATLEAST(3, 0, 0)
				false; // Drop the event in SDL3
#else
				0; // Drop the event in SDL2
#endif
		}
		else
		{
			last_event_time = current_time; // Update last event time only when an event is processed
		}
	}
	// For all other event types, always allow them
	return
#if SDL_VERSION_ATLEAST(3, 0, 0)
		true; // Allow the event in SDL3
#else
		1; // Allow the event in SDL2
#endif
}

inline void WakeGui()
{
	SDL_Event RedrawTriggeredEvent;
	RedrawTriggeredEvent.type = SDL_RegisterEvents(1);
	SDL_PushEvent(&RedrawTriggeredEvent);
}

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

	auto hasRequests()const
	{
		if (!no_sleep_requests)
			return false;
		return true;
	}

private:
	std::uint32_t no_sleep_requests = 0;
};

/*
	Volt uses a event driven sys and only draws upon request("only when necessary").
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

	bool shouldReDrawFrame()const
	{
		return redrawRequested;
	}

	~AdaptiveVsyncHandler()
	{
		stopRedrawSession();
	}

private:
	bool redrawRequested = false;
	int cache_frame_rate = 1;
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
	SDL_Texture* targetCache;
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

struct SDLResourceDeleter
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
		}
	}

	void operator()(SDL_Haptic *haptic) const
	{
		if (haptic != nullptr)
		{
			SDL_HapticClose(haptic);
			haptic = nullptr;
		}
	}
};

using UniqueTexture = std::unique_ptr<SDL_Texture, SDLResourceDeleter>;
using SharedTexture = std::shared_ptr<SDL_Texture>;
using UniqueHaptic = std::unique_ptr<SDL_Haptic, SDLResourceDeleter>;
using SharedHaptic = std::shared_ptr<SDL_Haptic>;

std::unique_ptr<SDL_Window, SDLResourceDeleter>
CreateUniqueWindow(const char *title, int w,
				   int h, uint32_t flags)
{
	return std::unique_ptr<SDL_Window, SDLResourceDeleter>(
		SDL_CreateWindow(title, 0, 0, w, h, flags), SDLResourceDeleter());
}

std::shared_ptr<SDL_Window>
CreateSharedWindow(const char *title, int w, int h, uint32_t flags)
{
	return std::shared_ptr<SDL_Window>(
		SDL_CreateWindow(title, 0, 0, w, h, flags), SDLResourceDeleter());
}

std::unique_ptr<SDL_Renderer, SDLResourceDeleter>
CreateUniqueRenderer(SDL_Window *window, int index, uint32_t flags)
{
	return std::unique_ptr<SDL_Renderer, SDLResourceDeleter>(
		SDL_CreateRenderer(window, NULL, flags), SDLResourceDeleter());
}

std::shared_ptr<SDL_Renderer>
CreateSharedRenderer(SDL_Window *window, int index, uint32_t flags)
{
	return std::shared_ptr<SDL_Renderer>(
		SDL_CreateRenderer(window, NULL, flags), SDLResourceDeleter());
}

inline UniqueTexture
CreateUniqueTexture(SDL_Renderer *renderer, Uint32 format, int access, const int w,
					const int h)
{
	return UniqueTexture(
		SDL_CreateTexture(renderer, format, access, w, h),
		SDLResourceDeleter());
}

SharedTexture
CreateSharedTexture(SDL_Renderer *renderer, Uint32 format, int access, const int &w,
					const int &h)
{
	return SharedTexture(
		SDL_CreateTexture(renderer, format, access, w, h),
		SDLResourceDeleter());
}

SharedTexture CreateSharedTextureFromSurface(SDL_Renderer *renderer, SDL_Surface *surface)
{
	return SharedTexture(
		SDL_CreateTextureFromSurface(renderer, surface),
		SDLResourceDeleter());
}

SharedTexture LoadSharedTexture(SDL_Renderer *renderer, const std::string &_img_path)
{
	return SharedTexture(
		IMG_LoadTexture(renderer, _img_path.c_str()),
		SDLResourceDeleter());
}

std::unique_ptr<SDL_Texture, SDLResourceDeleter>
CreateUniqueTextureFromSurface(SDL_Renderer *renderer, SDL_Surface *surface)
{
	return std::unique_ptr<SDL_Texture, SDLResourceDeleter>(SDL_CreateTextureFromSurface(renderer, surface));
}

inline UniqueHaptic
CreateUniqueHaptic(const int id)
{
	return UniqueHaptic(
		SDL_HapticOpen(id),
		SDLResourceDeleter());
}

SharedHaptic
CreateSharedHaptic(const int id = 0)
{
	return SharedHaptic(
		SDL_HapticOpen(id),
		SDLResourceDeleter());
}

void RenderClear(SDL_Renderer *renderer, const uint8_t &red, const uint8_t &green,
				 const uint8_t &blue,
				 const uint8_t &alpha = 0xFF)
{
	SDL_SetRenderDrawColor(renderer, red, green, blue, alpha);
	SDL_RenderClear(renderer);
}

auto DestroyTextureSafe = [](SDL_Texture *texture)
{
	if (texture != nullptr)
	{
		SDL_DestroyTexture(texture);
		texture = nullptr;
	}
};

// get %val of ref
template <typename T>
constexpr inline T to_cust(const T& val, const T& ref) { return ((val * ref) / static_cast<decltype(val)>(100)); }

void transformToRoundedTexture(SDL_Renderer* renderer, SDL_Texture* source_texture, float radius_percent);

class Haptics
{
public:
	Haptics()
	{
	}
	void create()
	{
		if (SDL_Init(SDL_INIT_HAPTIC) < 0)
		{
			GLogger.Log(Logger::Level::Error, "Failed to initialize SDL Haptic subsystem:", std::string(SDL_GetError()));
		}

		if (SDL_NumHaptics() < 1)
		{
			GLogger.Log(Logger::Level::Info, "No haptic devices found.");
		}

		haptic_dev = CreateSharedHaptic();
		if (haptic_dev.get() == nullptr)
		{
			GLogger.Log(Logger::Level::Error, "Failed to open haptic device:",std::string(SDL_GetError()));
			return;
		}

		GLogger.Log(Logger::Level::Info, "Haptic device opened:", std::string(SDL_HapticName(0)));

		// Check if the device supports a simple rumble effect.
		if (SDL_HapticQuery(haptic_dev.get()) & SDL_HAPTIC_LEFTRIGHT)
		{
			SDL_HapticEffect effect;
			SDL_memset(&effect, 0, sizeof(SDL_HapticEffect)); // Initialize to zero

			effect.type = SDL_HAPTIC_LEFTRIGHT;
			effect.leftright.length = 30;			  // duration
			effect.leftright.large_magnitude = 32767; // Max strength
			effect.leftright.small_magnitude = 32767; // Max strength

			// Upload the effect to the device
			effect_id = SDL_HapticNewEffect(haptic_dev.get(), &effect);
			if (effect_id < 0)
			{
				GLogger.Log(Logger::Level::Error, "Failed to create haptic effect:", std::string(SDL_GetError()));
			}
		}
		else
		{
			GLogger.Log(Logger::Level::Error,"Device does not support left/right rumble. You can try other effect types.");
		}
	}

	void play_effect()
	{
		if (haptic_dev.get() != nullptr && effect_id >= 0)
		{
			// Run the effect once. The '1' means it will play 1 time.
			if (SDL_HapticRunEffect(haptic_dev.get(), effect_id, 1) != 0)
			{
				GLogger.Log(Logger::Level::Error, "Failed to run haptic effect:", std::string(SDL_GetError()));
			} /*else {
			SDL_Log("Haptic effect played.");
		}*/
		}
	}

	~Haptics()
	{
		if (haptic_dev.get() != nullptr)
		{
			if (effect_id >= 0)
			{
				SDL_HapticDestroyEffect(haptic_dev.get(), effect_id);
			}
		}
		GLogger.Log(Logger::Level::Info, "Haptic device closed.");
	}

private:
	SharedHaptic haptic_dev = nullptr;
	int effect_id = -1;
};


class TextAttributes
{
public:
	TextAttributes() = default;

	TextAttributes(const std::string& a_text, const SDL_Color a_text_col,
		const SDL_Color& a_background_col) : text(a_text), text_color(a_text_col),
		bg_color(a_background_col) {
	}

	std::string text;
	SDL_Color bg_color;
	SDL_Color text_color;

	void setTextAttributes(const TextAttributes& text_attributes)
	{
		this->text = text_attributes.text;
		this->bg_color = text_attributes.bg_color;
		this->text_color = text_attributes.text_color;
	}

	TextAttributes& setText(const char* _text)
	{
		this->text = _text;
		return *this;
	}

	TextAttributes& setTextColor(const SDL_Color& _textcolor)
	{
		this->text_color = _textcolor;
		return *this;
	}

	TextAttributes& setTextBgColor(const SDL_Color& _text_bg_color)
	{
		this->bg_color = _text_bg_color;
		return *this;
	}
};

class FontAttributes
{
public:
	std::string font_file;
	uint8_t font_size = 0xff;
	FontStyle font_style;

	FontAttributes() = default;

	FontAttributes(const std::string& a_font_file, const FontStyle& a_font_style,
		const uint8_t& a_font_size) : font_file(a_font_file), font_size(a_font_size), font_style(a_font_style) {
	}

	void setFontAttributes(const FontAttributes& font_attributes)
	{
		this->font_file = font_attributes.font_file,
			this->font_size = font_attributes.font_size,
			this->font_style = font_attributes.font_style;
	}

	FontAttributes& setFontStyle(const FontStyle& a_fontstyle)
	{
		this->font_style = a_fontstyle;
		return *this;
	}

	FontAttributes& setFontFile(const std::string& a_fontfile)
	{
		this->font_file = a_fontfile;
		return *this;
	}

	FontAttributes& setFontSize(const uint8_t& a_fontsize)
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
	const unsigned char* font_data;
	unsigned int font_data_size;
	int font_size;
	std::string font_name;
};

MemFont RobotoBold{
	.font_data = ff_Roboto_Bold_ttf_data,
	.font_data_size = ff_Roboto_Bold_ttf_len,
	.font_size = 255,
	.font_name = "roboto-bold" };

std::unordered_map<Font, MemFont*> Fonts{
	//{Font::ConsolasBold, &ConsolasBold},
	//{Font::OpenSansRegular, &OpenSansRegular},
	//{{Font::OpenSansSemiBold, &OpenSansSemiBold},
	//{Font::OpenSansBold, &OpenSansBold},
	//{Font::OpenSansExtraBold, &OpenSansExtraBold},
	{Font::RobotoBold, &RobotoBold},
	//{Font::SegoeUiEmoji, &SegoeUiEmoji},
	//{Font::YuGothBold,&YuGothBold},
};

class FontStore
{
public:
	~FontStore()
	{
		for (auto& [path_, font] : fonts)
			TTF_CloseFont(font);
	}

	TTF_Font* operator[](const std::pair<std::string, int>& font)
	{
		const std::string key = font.first + std::to_string(font.second);
		if (fonts.count(key) == 0)
		{
			if (!(fonts[key] = TTF_OpenFont(font.first.c_str(), font.second)))
			{
				SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "%s", TTF_GetError());
				fonts.erase(key);
				return nullptr;
			}
			else
			{
				GLogger.Log(Logger::Level::Info, "New FileFont Loaded:", key);
				// TTF_SetFontSDF(fonts[key], SDL_TRUE);
				// TTF_SetFontOutline(fonts[key], m_font_outline);
				// TTF_SetFontKerning(fonts[key], 0);
				// TTF_SetFontHinting(fonts[key], TTF_HINTING_LIGHT_SUBPIXEL);
				// TTF_SetFontHinting(fonts[key], TTF_HINTING_MONO);
			}
		}
		return fonts[key];
	}

	TTF_Font* operator[](const MemFont& mem_font)
	{
		const std::string key = mem_font.font_name + std::to_string(mem_font.font_size);
		if (fonts.count(key) == 0)
		{
			// GLogger.Log(Logger::Level::Info, "FT loading " + key);
			SDL_RWops* rw_ = SDL_RWFromConstMem(mem_font.font_data, mem_font.font_data_size);
			auto* ft = TTF_OpenFontRW(rw_, SDL_TRUE, mem_font.font_size);
			if (!ft)
			{
				GLogger.Log(Logger::Level::Info, "TTF_OpenFontRW:", std::string(TTF_GetError()));
				return nullptr;
			}
			else
			{
				fonts[key] = ft;
				GLogger.Log(Logger::Level::Info, "New MemFont Loaded:", key);
				// TTF_SetFontSDF(fonts[key], SDL_TRUE);
				// TTF_SetFontOutline(fonts[key], m_font_outline);
				// TTF_SetFontKerning(fonts[key], 1);
				TTF_SetFontHinting(fonts[key], TTF_HINTING_MONO);
			}
		}
		return fonts[key];
	}

private:
	std::unordered_map<std::string, TTF_Font*> fonts;
};

class FontSystem
{
public:
	FontSystem(const FontSystem&) = delete;

	FontSystem(const FontSystem&&) = delete;

	static FontSystem& Get()
	{
		static FontSystem instance;
		return instance;
	}

	FontSystem& setFontFile(const char* font_file) noexcept
	{
		this->m_font_attributes.font_file = font_file;
		return *this;
	}

	FontSystem& setFontSize(const uint8_t& font_size) noexcept
	{
		this->m_font_attributes.font_size = font_size;
		return *this;
	}

	FontSystem& setFontStyle(const FontStyle& font_style) noexcept
	{
		this->m_font_attributes.font_style = font_style;
		return *this;
	}

	FontSystem& setCustomFontStyle(const int& custom_font_style) noexcept
	{
		this->m_custom_fontstyle = custom_font_style;
		return *this;
	}

	FontSystem& setFontAttributes(const FontAttributes& font_attributes,
		const int& custom_fontstyle = 0) noexcept
	{
		this->m_font_attributes = font_attributes;
		this->m_custom_fontstyle = custom_fontstyle;
		return *this;
	}

	FontSystem& setFontAttributes(const MemFont& _mem_ft, const FontStyle& ft_style_, const int& custom_fontstyle = 0) noexcept
	{
		this->m_font_attributes.setFontAttributes(FontAttributes{ _mem_ft.font_name, ft_style_, static_cast<uint8_t>(_mem_ft.font_size) });
		this->m_custom_fontstyle = custom_fontstyle;
		return *this;
	}

	TTF_Font* getFont(const std::string& font_file, const int& font_size)
	{
		return fontStore[{font_file, font_size}];
	}

	TTF_Font* getFont(const MemFont& _mem_ft)
	{
		return fontStore[_mem_ft];
	}

	std::optional<UniqueTexture> genTextTextureUnique(SDL_Renderer* renderer, const char* text, const SDL_Color text_color)
	{
		if (!genTextCommon())
			return {};
		SDL_Surface* textSurf = TTF_RenderUTF8_Blended(m_font, text, text_color);
		if (!textSurf)
			SDL_Log("%s", SDL_GetError());
		auto result = CreateUniqueTextureFromSurface(renderer, textSurf);
		if (!result.get())
			SDL_Log("%s", SDL_GetError());
		Async::GThreadPool.enqueue([](SDL_Surface* surface)
			{SDL_DestroySurface(surface); surface = nullptr; },
			textSurf);
		return result;
	}

	SDL_Texture* genTextTextureRaw(SDL_Renderer* renderer, const char* text, const SDL_Color text_color)
	{
		if (!genTextCommon())
			return {};
		SDL_Surface* textSurf = TTF_RenderUTF8_Blended(m_font, text, text_color);
		if (!textSurf)
			SDL_Log("%s", SDL_GetError());
		auto result = SDL_CreateTextureFromSurface(renderer, textSurf);
		if (!result)
			SDL_Log("%s", SDL_GetError());
		Async::GThreadPool.enqueue([](SDL_Surface* surface)
			{SDL_DestroySurface(surface); surface = nullptr; },
			textSurf);
		return result;
	}

	std::optional<UniqueTexture> genTextTextureUniqueV2(SDL_Renderer* renderer, const char* text, const SDL_Color text_color, float _w, float _h, bool wordWrap = false)
	{
		if (!genTextCommon())
			return {};
		std::vector<std::pair<SDL_FRect, SDL_Texture*>> finalGlyphs{};
		int maxW = 0, maxH = 0;
		int length = strlen(text);
		int textOffsetY = 0;
		int textOffsetX = 0;
		int lineHeight = TTF_FontLineSkip(m_font);

		for (int i = 0; i < length;)
		{
			int wordWidth = 0;
			int wordLength = 0;
			int advance;
			for (int j = i; j < length && text[j] != ' ' && text[j] != '\n'; ++j)
			{
				TTF_GlyphMetrics(m_font, text[j], NULL, NULL, NULL, NULL, &advance);
				wordWidth += advance;
				++wordLength;
			}

			if (wordWrap && textOffsetX + wordWidth > _w)
			{
				textOffsetX = 0;
				textOffsetY += lineHeight;
			}

			for (int j = 0; j < wordLength; ++j)
			{
				if (text[i + j] == '\n')
				{
					textOffsetX = 0;
					textOffsetY += lineHeight;
					break;
				}

				TTF_GlyphMetrics(m_font, text[i + j], NULL, NULL, NULL, NULL, &advance);
				SDL_Surface* textSurf = TTF_RenderGlyph_Blended(m_font, text[i + j], text_color);
				SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, textSurf);
				std::pair<SDL_FRect, SDL_Texture*> gl_{ {(float)textOffsetX, (float)textOffsetY, (float)textSurf->w, (float)textSurf->h}, texture };
				finalGlyphs.emplace_back(gl_);
				textOffsetX += advance;
				if (textOffsetX + textSurf->w >= maxW)
					maxW = textOffsetX + textSurf->w;
				Async::GThreadPool.enqueue([](SDL_Surface* surface)
					{SDL_DestroySurface(surface); surface = nullptr; }, textSurf);
			}

			if (text[i + wordLength] == ' ')
			{
				TTF_GlyphMetrics(m_font, ' ', NULL, NULL, NULL, NULL, &advance);
				textOffsetX += advance;
				++wordLength;
			}

			i += wordLength;
		}

		CacheRenderTarget crt_(renderer);
		auto result = CreateUniqueTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, maxW, (int)_h);
		SDL_SetTextureBlendMode(result.get(), SDL_BLENDMODE_BLEND);
		SDL_SetRenderTarget(renderer, result.get());
		RenderClear(renderer, 0, 0, 0, 0);
		for (auto& [grect, gtexture] : finalGlyphs)
		{
			RenderTexture(renderer, gtexture, NULL, (const SDL_FRect*)&grect);
			// SDL_DestroyTexture(gtexture);
			Async::GThreadPool.enqueue([](SDL_Texture* dtex)
				{SDL_DestroyTexture(dtex); dtex = nullptr; }, gtexture);
		}
		crt_.release(renderer);

		return result;
	}

	std::optional<SharedTexture> genTextTextureShared(SDL_Renderer* renderer, const char* text, const SDL_Color text_color)
	{
		if (!genTextCommon())
			return {};
		SDL_Surface* textSurf = TTF_RenderUTF8_Blended(m_font, text, text_color);
		if (!textSurf)
			SDL_Log("%s", SDL_GetError());
		auto result = CreateSharedTextureFromSurface(renderer, textSurf);
		if (!result.get())
			SDL_Log("%s", SDL_GetError());
		Async::GThreadPool.enqueue([](SDL_Surface* surface)
			{SDL_DestroySurface(surface); surface = nullptr; },
			textSurf);
		return result;
	}

	std::optional<UniqueTexture> u8GenTextTextureUnique(SDL_Renderer* renderer, const char8_t* text, const SDL_Color text_color)
	{
		if (!genTextCommon())
			return {};

		SDL_Surface* textSurf = TTF_RenderUTF8_Blended(m_font, (char*)text, text_color);
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
		this->m_font_attributes = FontAttributes{ "", FontStyle::Normal, 255 };
		m_font_outline = 0;
		m_custom_fontstyle = TTF_STYLE_NORMAL;
	}

	bool genTextCommon()
	{
		if (m_font_attributes.font_file.empty())
		{
			GLogger.Log(Logger::Level::Info, "FontSystem Error: Invlaid/empty fontfile!");
			return false;
		}

		m_font = fontStore[{m_font_attributes.font_file, m_font_attributes.font_size}];
		if (m_font == nullptr)
		{
			GLogger.Log(Logger::Level::Info, "Error generating text texture: NULL FONT");
			return false;
		}

		switch (m_font_attributes.font_style)
		{
			// using enum FontStyle;
		case FontStyle::Normal:
			TTF_SetFontStyle(m_font, TTF_STYLE_NORMAL);
			break;
		case FontStyle::Bold:
			TTF_SetFontStyle(m_font, TTF_STYLE_BOLD);
			break;
		case FontStyle::Italic:
			TTF_SetFontStyle(m_font, TTF_STYLE_ITALIC);
			break;
		case FontStyle::Underline:
			TTF_SetFontStyle(m_font, TTF_STYLE_UNDERLINE);
			break;
		case FontStyle::StrikeThrough:
			TTF_SetFontStyle(m_font, TTF_STYLE_STRIKETHROUGH);
			break;
		case FontStyle::BoldUnderline:
			TTF_SetFontStyle(m_font, TTF_STYLE_BOLD | TTF_STYLE_UNDERLINE);
			break;
		case FontStyle::BoldStrikeThrough:
			TTF_SetFontStyle(m_font, TTF_STYLE_BOLD | TTF_STYLE_STRIKETHROUGH);
			break;
		case FontStyle::ItalicBold:
			TTF_SetFontStyle(m_font, TTF_STYLE_ITALIC | TTF_STYLE_BOLD);
			break;
		case FontStyle::ItalicUnderline:
			TTF_SetFontStyle(m_font, TTF_STYLE_ITALIC | TTF_STYLE_UNDERLINE);
			break;
		case FontStyle::ItalicStrikeThrough:
			TTF_SetFontStyle(m_font, TTF_STYLE_ITALIC | TTF_STYLE_STRIKETHROUGH);
			break;
		case FontStyle::Custom:
			TTF_SetFontStyle(m_font, m_custom_fontstyle);
			break;
		default:
			TTF_SetFontStyle(m_font, TTF_STYLE_NORMAL);
			break;
		}

		return true;
	}

	TTF_Font* m_font;
	FontStore fontStore;
	FontAttributes m_font_attributes;
	int m_font_outline = 0;
	int m_custom_fontstyle;
};





struct IViewAttributes
{
	SDL_FRect bounds = {0.f, 0.f, 0.f, 0.f};
	SDL_FRect min_bounds = {0.f, 0.f, 1.f, 1.f};
	float rel_x = 0.f, rel_y = 0.f;
	std::string label = "nolabel";
	std::string type = "";
	std::string action = "default";
	std::string id = "null";
	bool required = false;
	bool is_form = false;
	bool prevent_default_behaviour = false;
	bool hidden = false;
	bool disabled = false;
};

class IView
{
public:
	IView() = default;
	virtual ~IView() = default;

public:
	SDL_FRect bounds = {0.f, 0.f, 0.f, 0.f};
	SDL_FRect min_bounds = {0.f, 0.f, 1.f, 1.f};
	float rel_x = 0.f, rel_y = 0.f;
	std::string label = "nolabel";
	std::string type = "";
	std::string action = "default";
	std::string id = "null";
	bool required = false;
	bool is_form = false;
	bool prevent_default_behaviour = false;
	bool hidden = false;
	bool disabled = false;
	bool auto_hide = false;
	// bool relative_pos = false;
	std::function<void()> onHideCallback = nullptr;
	std::vector<IView *> childViews;

public:
	IView *getView()
	{
		return this;
	}

	IView *getChildView(std::size_t child_index = 0)
	{
		return childViews[child_index];
	}

	IView *addChildView(IView *_child)
	{
		return childViews.emplace_back(_child);
	}

	virtual void attachRelativeView(IView *_prev_view, float _margin = 0.f)
	{
		bounds.y = _prev_view->bounds.y + _prev_view->bounds.h + _margin;
	}

	IView *clearAndAddChildView(IView *_child)
	{
		childViews.clear();
		return childViews.emplace_back(_child);
	}

	IView &clearChildViews()
	{
		childViews.clear();
		return *this;
	}

	SDL_FRect &getBoundsBox()
	{
		return bounds;
	}

	float getRealX()const { return rel_x + bounds.x; }

	float getRealY()const { return rel_y + bounds.y; }

	SDL_FPoint getRealPos()const { return {rel_x + bounds.x, rel_y + bounds.y}; }

	void setBoundsBox(const SDL_FRect &_bounds, const SDL_FRect &_min_bounds = {0.f})
	{
		bounds = _bounds;
	}

	virtual void updatePosBy(float dx, float dy)
	{
		bounds.x += dx, bounds.y += dy;
	};

	virtual void updatePos(float x, float y)
	{
		bounds.x = x, bounds.y = y;
	};

	virtual void updatePosX(float x)
	{
		bounds.x = x;
	};

	virtual void updatePosY(float y)
	{
		bounds.y = y;
	};

	void setOnHide(std::function<void()> _onHideCallback)
	{
		onHideCallback = _onHideCallback;
	}

	IView *toggleView()
	{
		if (hidden)
		{
			hidden = false;
			disabled = false;
		}
		else
		{
			hidden = true;
			disabled = true;
		}
		return this;
	}

	IView *hide()
	{
		for (auto child : childViews)
			child->hide();
		hidden = true;
		if (onHideCallback)
			onHideCallback();
		return this;
	}

	bool isHidden() const { return hidden; }

	IView *show()
	{
		hidden = false;
		return this;
	}

	IView *disable()
	{
		for (auto child : childViews)
			child->disable();
		disabled = true;
		return this;
	}
	IView *enable()
	{
		for (auto child : childViews)
			child->enable();
		disabled = false;
		return this;
	}

	bool isDisabled()const { return disabled; }

	virtual bool handleEvent() = 0;

	virtual void onUpdate() {};

	virtual void draw() = 0;

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



class MonoViewGroup :public IView{
public:
	auto addView(const std::string& label, IView* iview)
	{
		return views[label]=iview;
	}

	/*auto removeView(const std::string& label)
	{
		if (views.contains(label))
		{
			views.erase(views.begin() + indexer[label]);
		}
	}*/

	IView* setActiveView(const std::string& label) {
		active_vw = views.at(label);
		return active_vw;
	}

	IView* getActiveView() {
		return active_vw;
	}

	bool isEmpty()
	{
		return views.empty();
	}

	bool handleEvent()override final
	{
		if (not hidden and not (nullptr == active_vw))
		{
			return active_vw->handleEvent();
		}
		return false;
	}

	void onUpdate()override final
	{
		if (not hidden and not (nullptr == active_vw))
		{
			active_vw->onUpdate();
		}
	};

	void forceDrawAll()
	{
		for (auto& [label,view] : views)
			view->draw();
	}

	void draw()override final
	{
		if (not hidden and not (nullptr == active_vw))
		{
			active_vw->draw();
		}
	}
private:
	IView* active_vw=nullptr;
	std::unordered_map<std::string, IView*> views;
};



class ViewTree
{
public:
	auto addView(const std::string &label, IView *iview)
	{
		indexer[label] = view_tree.size();
		return view_tree.emplace_back(iview);
	}

	auto removeView(const std::string &label)
	{
		if (indexer.contains(label))
		{
			view_tree.erase(view_tree.begin() + indexer[label]);
			indexer.erase(label);
		}
	}

	bool isEmpty()
	{
		return view_tree.empty();
	}

	void toggleView(const std::string &label)
	{
		if (indexer.contains(label))
		{
			if (view_tree[indexer[label]]->isHidden())
			{
				view_tree[indexer[label]]->show();
				view_tree[indexer[label]]->enable();
			}
			else
			{
				view_tree[indexer[label]]->hide();
				view_tree[indexer[label]]->disable();
			}
		}
	}

	void showAndEnable(const std::string &label)
	{
		if (indexer.contains(label))
		{
			view_tree[indexer[label]]->show();
			view_tree[indexer[label]]->enable();
		}
	}

	void hideAndDisable(const std::string &label)
	{
		if (indexer.contains(label))
		{
			view_tree[indexer[label]]->hide();
			view_tree[indexer[label]]->disable();
		}
	}

	auto setViewHidden(const std::string &label, bool _hidden)
	{
		if (indexer.contains(label))
			_hidden ? view_tree[indexer[label]]->hide() : view_tree[indexer[label]]->show();
	}

	auto setViewDisabled(const std::string &label, bool _disabled)
	{
		if (indexer.contains(label))
			_disabled ? view_tree[indexer[label]]->disable() : view_tree[indexer[label]]->enable();
	}

	auto setTreeHidden(bool _hidden)
	{
		hidden_ = _hidden;
	}

	auto setTreeDisabled(bool _disabled)
	{
		disabled_ = _disabled;
	}

	bool handleEvent()
	{
		if (not hidden_ /*and not disabled_*/)
		{
			for (auto view_index = view_tree.size(); view_index > 0; --view_index)
			{
				auto &iv = view_tree[view_index - 1];
				if (not iv->isHidden())
					if (iv->handleEvent())
						return true;
			}
		}
		return false;
	}

	void forceHandleEventAll()
	{
		for (auto view_ : view_tree)
			view_->handleEvent();
	}

	void onUpdate()
	{
		if (not hidden_)
		{
			for (int view_index = 0; view_index < view_tree.size(); ++view_index)
			{
				auto &view = view_tree[view_index];
				if (not view->isHidden())
					view->onUpdate();
			}
		}
	};

	void forceDrawAll()
	{
		for (auto view : view_tree)
			view->draw();
	}

	void draw()
	{
		if (not hidden_)
		{
			for (int view_index = 0; view_index < view_tree.size(); ++view_index)
			{
				auto &view = view_tree[view_index];
				if (not view->isHidden())
					view->draw();
			}
		}
	}

public:
	IView *operator[](const std::string &label)
	{
		if (indexer.contains(label))
			return view_tree[indexer[label]];
		return nullptr;
	}

private:
	std::unordered_map<std::string, size_t> indexer;
	std::vector<IView *> view_tree;
	bool hidden_ = false, disabled_;
	// ViewTree child_tree();
};

class Application;

class Context
{
public:
	SDL_Renderer *renderer;
	SDL_Window *window;
	Application* app;
	AdaptiveVsync *adaptiveVsync;
	SDL_Event *event;
	Haptics *haptics;
	// parent view
	IView *pv;
	IView *cv;
	bool skipFrame = false;

	Context() = default;

	Context(Context *_context)
	{
		setContext(_context);
	}

	void setContext(Context *_context, IView *parent = nullptr)
	{
		renderer = _context->renderer;
		window = _context->window;
		if (parent == nullptr)
		{
			pv = _context->cv;
		}
		else
		{
			pv = parent;
		}
		/*
		cv->rel_x=pv->rel_x+pv->bounds.x;
		cv->rel_y=pv->rel_y+pv->bounds.y;*/
		adaptiveVsync = _context->adaptiveVsync;
		event = _context->event;
		haptics = _context->haptics;
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

class CharStore :public Context {
public:
	CharStore() = default;
	void setProps(Context* cntx, FontAttributes& fa, int _custom_ft_style = 0) {
		Context::setContext(cntx);
		fattr = fa;
		custom_ft_style = _custom_ft_style;
	}

	SDL_Texture* getChar(std::string& _txt, SDL_Color& color) {
		if (_txt.empty()) {
			GLogger.Log(Logger::Level::Error, "CharStore::getChar invoked with empty string!");
			return nullptr;
		}
		if (not char_textures.contains(_txt)) {
			FontSystem::Get().setFontAttributes(fattr, custom_ft_style);
			auto textTex = FontSystem::Get().genTextTextureRaw(renderer, _txt.c_str(), color);
			if (textTex != nullptr) {
				char_textures[_txt] = textTex;
			}
			else {
				GLogger.Log(Logger::Level::Error, "CharStore::getChar::genText returned null!");
				return nullptr;
			}
		}
		return char_textures[_txt];
	}

	~CharStore() {
		for (auto& [text, texture] : char_textures) {
			SDL_DestroyTexture(texture);
		}
	}
private:
	std::unordered_map<std::string, SDL_Texture*> char_textures{};
	FontAttributes fattr{};
	int custom_ft_style = 0;
};

class ToastManager : public Context {
public:
	void Build(Context* _cntx, FontAttributes _fattr, SDL_FRect _app_bounds) {
		setContext(_cntx);
		vsync.setAdaptiveVsync(_cntx->adaptiveVsync);
		fattr = _fattr;
		fattr.font_size = std::clamp(fattr.font_size, (uint8_t)0, (uint8_t)254);
		app_bounds = _app_bounds;

		if (fattr.font_file.empty())
		{
			Fonts[mem_font]->font_size = fattr.font_size;
			fattr.font_file = Fonts[mem_font]->font_name;
			//tmpFont = FontSystem::Get().getFont(*Fonts[mem_font]);
		}
		char_store.setProps(getContext(), fattr);
		GLogger.Log(Logger::Level::Info, "Toast FTS:", (uint8_t)fattr.font_size);
	}

	void addToast(std::string message, uint64_t duration=3000, SDL_Color bg_col = { 255,255,255,200 }, SDL_Color txt_col = { 0,0,0,255 }, float corner_radius = 25.f) {
		auto capped_duration = std::clamp(duration, (uint64_t)1, (uint64_t)3000);
		std::vector<std::vector<SDL_Texture*>> textures{};
		std::vector<float> heights{};
		float max_w = to_cust(80.f,app_bounds.w);
		float sum_w = 0.f, sum_h = 0.f;
		float max_ln_h = 0.f;
		std::size_t line = 0;
		textures.push_back({});
		for (auto& ch : message) {
			std::string outStr = { ch };
			auto ch_texture = char_store.getChar(outStr, txt_col);
			int tmp_w, tmp_h;
			SDL_QueryTexture(ch_texture, nullptr, nullptr, &tmp_w, &tmp_h);
			sum_w += (float)tmp_w;
			max_ln_h = std::max(max_ln_h, (float)tmp_h);
			if (sum_w > max_w) {
				GLogger.Log(Logger::Level::Info, "Toast Line Height:", max_ln_h);
				line++;
				sum_w = (float)tmp_w;
				sum_h += (float)max_ln_h;
				heights.push_back(max_ln_h);
				max_ln_h = 0.f;
				textures.push_back({});
			}
			textures[line].push_back(ch_texture);
		}

		float tw = textures.size() == 1 ? sum_w : max_w;
		float th = textures.size() == 1 ? max_ln_h : sum_h;

		SharedTexture ttexr = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, (int)tw, (int)th);
		CacheRenderTarget crt(renderer);
		SDL_SetRenderTarget(renderer, ttexr.get());
		SDL_SetRenderDrawColor(renderer, bg_col.r, bg_col.g, bg_col.b, bg_col.a);
		SDL_RenderClear(renderer);
		max_ln_h = 0.f;
		sum_w = 0.f;
		sum_h = 0.f;
		SDL_FRect ch_dst{ 0.f,0.f,100.f,100.f };
		for (auto& vec_txr : textures) {
			for (auto txr : vec_txr) {
				int tmp_w, tmp_h;
				SDL_QueryTexture(txr, nullptr, nullptr, &tmp_w, &tmp_h);
				
				ch_dst.x = sum_w;
				ch_dst.y = sum_h;
				ch_dst.w = (float)tmp_w;
				ch_dst.h = (float)tmp_h;
				RenderTexture(renderer, txr, nullptr, &ch_dst);
				sum_w += (float)tmp_w;
				max_ln_h = std::max(max_ln_h, (float)tmp_h);
			}
			sum_h += max_ln_h;
			max_ln_h = 0.f;
			sum_w = 0.f;
		}
		crt.release(renderer);
		transformToRoundedTexture(renderer, ttexr.get(), corner_radius);
		ch_dst = {
			app_bounds.x + ((app_bounds.w - tw) / 2.f),
			app_bounds.h-to_cust(20.f,app_bounds.h)-th,
			tw, th
		};
		toast_msgs.push_back({ SDL_GetTicks64() - trans_duration, SDL_GetTicks64(), ch_dst,std::move(ttexr) });
		if (not toast_msgs.empty()) {
			vsync.startRedrawSession();
		}
	}

	void onUpdate() {
		//SDL_GetTicks64()
	}

	void draw() {
		if (not toast_msgs.empty()) {
			auto& [strt, time, rect, txr] = toast_msgs.front();
			const auto elapsed_pause_duration = SDL_GetTicks64() - strt;
			if (elapsed_pause_duration >= trans_duration) {
				const auto elapsed = SDL_GetTicks64() - time;
				RenderTexture(renderer, txr.get(), nullptr, &rect);
				if (elapsed >= 3000) {
					toast_msgs.pop_front();
					// if not empty update/reset the next entity start time
					if (not toast_msgs.empty()) {
						auto& [nxt_strt, nxt_time, nxt_rect, nxt_txr] = toast_msgs.front();
						nxt_strt = SDL_GetTicks64();
						nxt_time = SDL_GetTicks64() + trans_duration;
					}
				}
			}
		}
		else {
			vsync.stopRedrawSession();
		}
	}

private:
	// toast msg transition duration
	uint64_t trans_duration = 250;
	SDL_Color bg_col{ 255,255,255,200 };
	// <start_time, duration, rect, texture>
	std::deque<std::tuple<uint64_t, uint64_t, SDL_FRect, SharedTexture>> toast_msgs{};
	CharStore char_store{};
	FontAttributes fattr{};
	Font mem_font = Font::RobotoBold;
	SDL_FRect app_bounds{0.f,0.f,480.f,720.f};
	AdaptiveVsyncHandler vsync{};
};


class DisplayInfo : Context
{
public:
	DisplayInfo(const DisplayInfo &) = delete;
	DisplayInfo(const DisplayInfo &&) = delete;

	using Context::setContext;

	static DisplayInfo &Get()
	{
		static DisplayInfo instance;
		return instance;
	}

	DeviceDisplayType GetDeviceDisplayType()const
	{
		return display_type;
	}

	void initDisplaySystem(const float &display_index = 0.f)
	{
		// SDL_GetDisplayDPI() - not reliable across platforms, approximately replaced by multiplying `display_scale` in the structure returned by SDL_GetDesktopDisplayMode() times 160 on iPhone and Android, and 96 on other platforms.
		/*auto *_mode = SDL_GetCurrentDisplayMode(0);
			int windowWidth, windowHeight;
			int drawableWidth, drawableHeight;
			SDL_GetRendererInfo(renderer, &rendererInfo);
			SDL_Log("MaxTextureW:%d MaxTextureH:%d", rendererInfo.max_texture_width, rendererInfo.max_texture_height);

			SDL_GetWindowSize(window, &windowWidth, &windowHeight);
			SDL_Log("WINDOW_SIZE H: %d W: %d", windowHeight, windowWidth);
			RenderW = windowWidth;
			RenderH = windowHeight;
			int dispCounts, dc2;
			auto dm = SDL_GetCurrentDisplayMode(1);
			auto ads = SDL_GetDisplays(&dispCounts);
			auto dcs = SDL_GetDisplayContentScale(1);
			auto pd = SDL_GetWindowPixelDensity(window);
			auto ds = SDL_GetWindowDisplayScale(window);
			auto fdm = SDL_GetFullscreenDisplayModes(1, &dc2);
			float dpiScale = (float)dm->w / (float)windowWidth;
			H_DPI = dpiScale;
			dpiScale = (float)dm->h / (float)windowHeight;
			V_DPI = dpiScale;
			auto *ddm = SDL_GetDesktopDisplayMode(1);
			DDPI = 160.f * ds;
			#ifdef _WIN32
			DDPI = 96.f * ds;
			#endif
*/
		DDPI = 160.f;
#ifdef _WIN32
		DDPI = 96.f;
#endif
		/*GLogger.Log(Logger::Level::Info, "PixelDensity:" + DDPI + " DispScale:" + ds + " DdmScale:" + ddm->pixel_density + " DispContentScale:" + dcs + " HDPI:" + H_DPI + " VDPI:" + V_DPI + " Mode.W:" + dm->w + " Mode.H:" + dm->h);*/
	}

	void handleEvent()
	{
		if (event->type == EVT_WPSC or event->type == EVT_WMAX)
		{
			OldRenderW = RenderW;
			OldRenderH = RenderH;
			int newW, newH;
			SDL_GetWindowSize(window, &newW, &newH);
			RenderW = static_cast<float>(newW);
			RenderH = static_cast<float>(newH);
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
	constexpr inline T to_cust(const T& val, const T& ref) const { return ((val * ref) / static_cast<decltype(val)>(100)); }

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
		display_type = DeviceDisplayType::Unknown;
		DrawableH = 0, DrawableW = 0;
		RenderH = RenderW = DPRenderH = DPRenderW = DDPI = H_DPI = V_DPI = V_DPI_R = H_DPI_R = 1.f;
	}
};

class Application : protected Context, public IView
{
public:
	struct Config
	{
		std::string title{"Volt Application"};
		int x=100,y=50, w = 320, h = 500;
		int window_flags = HIGH_PIXEL_DENSITY;
		int renderer_flags = SDL_RENDERER_ACCELERATED /*| SDL_RENDERER_PRESENTVSYNC*/;
		bool init_ttf = true;
		bool init_img = true;
		bool init_everyting = true;
		bool mouse_touch_events = true;
		std::string logs_dir = "";
		float toast_ft_size = 2.5f;// px
	};

public:
	using Context::adaptiveVsync;
	using Context::event;
	using Context::getContext;
	using Context::renderer;
	using Context::skipFrame;
	using Context::window;
	using IView::bounds;
	using IView::ph;
	using IView::pw;
	using IView::to_cust;

public:
	AdaptiveVsync adaptiveVsync_;
	SDL_Event RedrawTriggeredEvent_;
	UniqueTexture texture;
	std::string PrefLocale, CurrentLocale;
	std::string BasePath{}, PrefPath{};
	bool quit = false;
	bool show_fps = false;
	std::ofstream file;
	std::ofstream out_app_props_file;
	Config cfg;
	ToastManager toast_mgr{};

public:
	Application()
	{
		std::setlocale(LC_ALL, "en_US.UTF-8");
	}

public:
	short create(Application::Config config)
	{
		cfg = config;
		file.open(config.logs_dir+config.title + "_logs.txt", std::ios::out | std::ios::app);
		if (file.is_open())
		{
			file << "\n\n\n\n----------NEW LOGGING SESSION----------\n";
		}
		GLogger.onLog([this](std::string slog)
					  {
			SDL_Log("%s", slog.c_str());
			if (file.is_open())
			{
				file << slog << "\n";
			} });
		GLogger.Log(Logger::Level::Info, "sys std::thread::hardware_concurrency:",std::to_string(std::thread::hardware_concurrency()));

		int wnx = -1, wny=-1;
		if (std::filesystem::exists(config.title + "_props.txt")) {
			std::ifstream app_props_file;
			std::string line;
			app_props_file.open(config.title + "_props.txt", std::ios::in);
			if (!app_props_file.is_open())
			{
				GLogger.Log(Logger::Level::Error, "Failed to open file:", config.title + "_props.txt");
				return false;
			}
			while (std::getline(app_props_file, line))
			{
				std::stringstream ss(line);
				std::string segment;
				std::vector<std::string> values;

				while (std::getline(ss, segment, ':'))
				{
					values.push_back(segment);
				}
				if (values.size() >= 1)
				{
					if (values[0] == "winpos")
					{
						wnx = std::atoi(values[1].c_str());
						wny = std::atoi(values[2].c_str());
						cfg.x = wnx;
						cfg.y = wny;

					}
				}
			}
		}
		//out_app_props_file.open(config.title + "_props.txt", std::ios::out);
		//if (not out_app_props_file.is_open())
		//{
		//	GLogger.Log(Logger::Level::Error, "Failed to open file:", config.title + "_props.txt");
		//	//file << "\n\n\n\n----------NEW LOGGING SESSION----------\n";
		//}

		if (config.mouse_touch_events)
			SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");
		if (config.init_everyting)
			SDL_Init(SDL_INIT_EVERYTHING);
		if (config.init_ttf)
			TTF_Init();
		if (config.init_img)
			IMG_Init(IMG_INIT_PNG);
		// Set the event filter
		// SDL_SetEventFilter(myEventFilter, nullptr); // Pass nullptr if no user data is needed

		//window = CreateWindow(config.title.c_str(), config.w, config.h, config.window_flags);
		window = SDL_CreateWindow(config.title.c_str(), cfg.x, cfg.y, config.w, config.h, config.window_flags);
		SDL_Rect usb_b{0};
		SDL_GetWindowSize(window, &usb_b.w, &usb_b.h);
#ifdef _MSC_VER
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "direct3d");
#else
		SDL_SetHint(SDL_HINT_RENDER_DRIVER, "opengl");
#endif
		renderer = SDL_CreateRenderer(window, NULL, config.renderer_flags);
		DisplayInfo::Get().setContext(this);
		DisplayInfo::Get().initDisplaySystem(0);
		DisplayInfo::Get().RenderW = usb_b.w;
		DisplayInfo::Get().RenderH = usb_b.h;
		bounds.x = 0.f;
		bounds.y = 0.f;
		bounds.w = DisplayInfo::Get().RenderW;
		bounds.h = DisplayInfo::Get().RenderH;
		pv = this;
		cv = this;
#ifndef OLDSDL
// SDL_SetHint(SDL_HINT_RENDER_LINE_METHOD, "1");
//  SDL_SetHint(SDL_HINT_RENDER_LINE_METHOD, "3");
#endif

		if (wnx > -1) {
			SDL_SetWindowPosition(window, wnx, wny);
		}

		adaptiveVsync = &adaptiveVsync_;
		event = &event_;
		haptics_.create();
		haptics = &haptics_;
		RedrawTriggeredEvent = &RedrawTriggeredEvent_;
		RedrawTriggeredEvent->type = SDL_RegisterEvents(1);
		SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
		DisplayInfo::Get().setContext(this);

		FontAttributes tst_ft{};
		tst_ft.font_size = IView::to_cust(config.toast_ft_size, bounds.h);
		toast_mgr.Build(getContext(), tst_ft, bounds);

		/*BasePath = SDL_GetBasePath();
			PrefPath = SDL_GetPrefPath("Volt", config.title.c_str());
			GLogger.setOutputFile(PrefPath + "logs-" + getDateAndTimeStr() + ".");
			GLogger.Log(Logger::Level::Info, "BasePath:" + BasePath);
			GLogger.Log(Logger::Level::Info, "PrefPath:" + PrefPath);
			*/
		return 1;
	}

	void run()
	{
		tmPrevFrame = SDL_GetTicks();
		this->loop();
	}

	bool handleEvent() override
	{
		switch (event->type)
		{
		case EVT_QUIT:
			quit = true;
			break;
		case SDL_WINDOWEVENT:
		{
			if (event->window.type == SDL_WINDOWEVENT_MOVED) {
				GLogger.Log(Logger::Level::Info, "window moved");
			}
			out_app_props_file.open(cfg.title + "_props.txt", std::ios::out);
			if (out_app_props_file.is_open())
			{
				//GLogger.Log(Logger::Level::Error, "Failed to open file:", config.title + "_props.txt");
				int wx =0, wy = 0;
				SDL_GetWindowPosition(window, &wx, &wy);
				out_app_props_file << "winpos:" << wx << ":" << wy;
			}
			out_app_props_file.close();
			break;
		}
		/*case SDL_KEYDOWN:
			quit = true;
			break;*/
		default:
			break;
		}

		return false;
	}

	void draw() override = 0;

	auto getFPS() const
	{
		return fps;
	}

	void showToast(std::string message, uint64_t duration = 3000, SDL_Color bg_col = { 255,255,255,205 }, SDL_Color txt_col = { 0,0,0,255 }, float corner_radius = 25.f) {
		toast_mgr.addToast(message, duration, bg_col, txt_col, corner_radius);
		WakeGui();
	}

	~Application()
	{
        file.close();
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
			if (adaptiveVsync->pollEvent(event) != 0)
			{
				Application::handleEvent();
				this->handleEvent();
			}
			if (not skipFrame)
			{
				onUpdate();
				this->draw();
				toast_mgr.draw();
				SDL_RenderPresent(renderer);
			}
			skipFrame = false;
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
	Haptics haptics_;
	SDL_Event event_;
	uint32_t tmPrevFrame = 0;
	uint32_t tmNowFrame = 0;
	uint32_t frames = 0;
	uint32_t fps = 0;
};

template <typename T>
struct Point
{
	Point(T _x, T _y) : x(_x), y(_y) {}
	T x, y;
};

struct Spline
{
	std::deque<SDL_FPoint> points;

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

inline constexpr auto lerp_colors(const float colors[2][3], const float &value)
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
		SDL_RenderDrawLine(renderer, _rect.x + x, _rect.y, _rect.x + x, _rect.y + _rect.h);
		t += dt;
	}
}

inline auto rotate(const std::array<float, 2> &position, const float &angle) noexcept
{
	return std::array{
		cosf(angle) * position[0] - sinf(angle) * position[1],
		sinf(angle) * position[0] + cosf(angle) * position[1]};
}

inline constexpr auto translate(const std::array<float, 2> &position, const float &x, const float &y) noexcept
{
	return std::array{position[0] + x, position[1] + y};
}

void fillGradientRectAngle(SDL_Renderer *renderer, const SDL_FRect &_rect, const float &_angle, const SDL_Color &_left, const SDL_Color &_right)
{
	const float rt = 1.f / 255.f;
	float norm_colors[2][3] = {{rt * _left.r, rt * _left.g, rt * _left.b}, {rt * _right.r, rt * _right.g, rt * _right.b}};

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
			SDL_RenderDrawPoint(renderer, _rect.x + x, _rect.y + y);
		}
	}
}

void fillGradientTexture(SDL_Renderer *renderer, SDL_Texture *_texture, const float &_angle, const SDL_Color &_left, const SDL_Color &_right)
{
	int w, h, pitch;
	SDL_QueryTexture(_texture, 0, 0, &w, &h);
	const float rt = 1.f / 255.f;
	const float norm_colors[2][3] = {{rt * _left.r, rt * _left.g, rt * _left.b}, {rt * _right.r, rt * _right.g, rt * _right.b}};
	// float t = 0.f;
	// const float dt = 1.f / w;
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
}

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

// Function for loading an image to SDL_Texture
static SDL_Texture *loadImage(SDL_Renderer *renderer, const char *path)
{
	SDL_Surface *img = IMG_Load(path);
	// blurIMG(img,1);
	if (img == NULL)
	{
		fprintf(stderr, "IMG_Load Error: %s\n", IMG_GetError());
		return NULL;
	}
	SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, img);
	SDL_FreeSurface(img);
	if (texture == NULL)
	{
		fprintf(stderr, "SDL_CreateTextureFromSurface Error: %s\n", SDL_GetError());
		return NULL;
	}
	return texture;
}

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
	RenderPoint(_renderer, static_cast<int>(_x), static_cast<int>(_y));
}

void drawPixelWeight(SDL_Renderer *_renderer, const int &_x, const int &_y, const float &_weight, const SDL_Color &_color)
{
	const uint8_t alpha_ = static_cast<uint8_t>(_weight * static_cast<float>(_color.a));
	SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
	RenderPoint(_renderer, _x, _y);
}

// Function to rotate a point around the center
SDL_FPoint rotatePoint(SDL_FPoint point, SDL_FPoint center, double angle)
{
	double radians = angle * 3.1415926 / 180.0;
	double s = std::sin(radians);
	double c = std::cos(radians);

	// Translate point back to origin
	point.x -= center.x;
	point.y -= center.y;

	// Rotate point
	double xnew = point.x * c - point.y * s;
	double ynew = point.x * s + point.y * c;

	// Translate point back
	point.x = xnew + center.x;
	point.y = ynew + center.y;

	return point;
}

void draw_ring(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r, const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
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
				bias = std::clamp(((1.f - ((fabs(mid1 - res) * 2.f) / max)) * (max1 / 2.f)), 0.f, 1.f);
				points_.emplace_back(SDL_FPoint{(_x - x), (_y - y)});
				points_.emplace_back(SDL_FPoint{(_x + x), (_y - y)});
				points_.emplace_back(SDL_FPoint{(_x - x), (_y + y)});
				points_.emplace_back(SDL_FPoint{(_x + x), (_y + y)});
				if (bias != prev_bias)
				{
					const uint8_t alpha_ = static_cast<uint8_t>(prev_bias * static_cast<float>(_color.a));
					SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
					RenderPoints(_renderer, points_.data(), points_.size() - 4);
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
					RenderPoints(_renderer, points_.data(), points_.size() - 4);
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

/*
void draw_ring_top_left_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r,
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
				drawPixelFWeight(_renderer, _x - x, _y - y, bias, _color);
			}
		}
	}
}

void draw_ring_top_right_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r,
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
				drawPixelFWeight(_renderer, _x + x, _y - y, bias, _color);
			}
		}
	}
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
	if (_quadrant == QUADRANT::TOP_LEFT)
		draw_ring_top_left_quadrant(_renderer, _x, _y, _inner_r, _outer_r, _color);
	else if (_quadrant == QUADRANT::TOP_RIGHT)
		draw_ring_top_right_quadrant(_renderer, _x, _y, _inner_r, _outer_r, _color);
	else if (_quadrant == QUADRANT::BOTTOM_LEFT)
		draw_ring_bottom_left_quadrant(_renderer, _x, _y, _inner_r, _outer_r, _color);
	else if (_quadrant == QUADRANT::BOTTOM_RIGHT)
		draw_ring_bottom_right_quadrand(_renderer, _x, _y, _inner_r, _outer_r, _color);
}*/

void draw_ring_top_left_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r,
								 const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
{
	const float outer_r2_ = _outer_r * _outer_r;
	const float inner_r2_ = _inner_r * _inner_r;
	const float mid1 = inner_r2_ + ((outer_r2_ - inner_r2_) / 2.f);
	const float max = (outer_r2_ - inner_r2_);
	const float max1 = (_outer_r - _inner_r);

	for (float y = 0.5f; y <= _outer_r; y += 1.f)
	{
		for (float x = 0.5f; x <= _outer_r; x += 1.f)
		{
			float res = x * x + y * y;
			if (res <= outer_r2_ && res >= inner_r2_)
			{
				float bias = std::clamp(((1.f - ((fabs(mid1 - res) * 2.f) / max)) * (max1 / 2.f)), 0.f, 1.f);
				drawPixelFWeight(_renderer, _x - x, _y - y, bias, _color);
			}
		}
	}
}

void draw_ring_top_right_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r,
								  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
{
	const float outer_r2_ = _outer_r * _outer_r;
	const float inner_r2_ = _inner_r * _inner_r;
	const float mid1 = inner_r2_ + ((outer_r2_ - inner_r2_) / 2.f);
	const float max = (outer_r2_ - inner_r2_);
	const float max1 = (_outer_r - _inner_r);

	for (float y = 0.5f; y <= _outer_r; y += 1.f)
	{
		for (float x = 0.5f; x <= _outer_r; x += 1.f)
		{
			float res = x * x + y * y;
			if (res <= outer_r2_ && res >= inner_r2_)
			{
				float bias = std::clamp(((1.f - ((fabs(mid1 - res) * 2.f) / max)) * (max1 / 2.f)), 0.f, 1.f);
				drawPixelFWeight(_renderer, _x + x, _y - y, bias, _color);
			}
		}
	}
}

void draw_ring_bottom_left_quadrant(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r,
									const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
{
	const float outer_r2_ = _outer_r * _outer_r;
	const float inner_r2_ = _inner_r * _inner_r;
	const float mid1 = inner_r2_ + ((outer_r2_ - inner_r2_) / 2.f);
	const float max = (outer_r2_ - inner_r2_);
	const float max1 = (_outer_r - _inner_r);

	for (float y = 0.5f; y <= _outer_r; y += 1.f)
	{
		for (float x = 0.5f; x <= _outer_r; x += 1.f)
		{
			float res = x * x + y * y;
			if (res <= outer_r2_ && res >= inner_r2_)
			{
				float bias = std::clamp(((1.f - ((fabs(mid1 - res) * 2.f) / max)) * (max1 / 2.f)), 0.f, 1.f);
				drawPixelFWeight(_renderer, _x - x, _y + y, bias, _color);
			}
		}
	}
}

void draw_ring_bottom_right_quadrand(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r,
									 const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
{
	const float outer_r2_ = _outer_r * _outer_r;
	const float inner_r2_ = _inner_r * _inner_r;
	const float mid1 = inner_r2_ + ((outer_r2_ - inner_r2_) / 2.f);
	const float max = (outer_r2_ - inner_r2_);
	const float max1 = (_outer_r - _inner_r);

	for (float y = 0.5f; y <= _outer_r; y += 1.f)
	{
		for (float x = 0.5f; x <= _outer_r; x += 1.f)
		{
			float res = x * x + y * y;
			if (res <= outer_r2_ && res >= inner_r2_)
			{
				float bias = std::clamp(((1.f - ((fabs(mid1 - res) * 2.f) / max)) * (max1 / 2.f)), 0.f, 1.f);
				drawPixelFWeight(_renderer, _x + x, _y + y, bias, _color);
			}
		}
	}
}
void draw_ring_quadrand(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_inner_r, const float &_outer_r,
						const QUADRANT &_quadrant, const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
{
	switch (_quadrant)
	{
	case QUADRANT::TOP_LEFT:
		draw_ring_top_left_quadrant(_renderer, _x, _y, _inner_r, _outer_r, _color);
		break;
	case QUADRANT::TOP_RIGHT:
		draw_ring_top_right_quadrant(_renderer, _x, _y, _inner_r, _outer_r, _color);
		break;
	case QUADRANT::BOTTOM_LEFT:
		draw_ring_bottom_left_quadrant(_renderer, _x, _y, _inner_r, _outer_r, _color);
		break;
	case QUADRANT::BOTTOM_RIGHT:
		draw_ring_bottom_right_quadrand(_renderer, _x, _y, _inner_r, _outer_r, _color);
		break;
	}
}

// Assuming RenderPoints is defined elsewhere, e.g.:
// void RenderPoints(SDL_Renderer* renderer, const SDL_FPoint* points, size_t count);
/*
void draw_filled_circle_optimized(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
								  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
{
	const float r2_ = _r * _r;
	const float r_4 = _r / 2.f;

	// Convert scalar constants to NEON vectors
	const float32x4_t r2_vec = vdupq_n_f32(r2_);
	const float32x4_t r_4_vec = vdupq_n_f32(r_4);
	const float32x4_t one_f_vec = vdupq_n_f32(1.f);
	const float32x4_t zero_f_vec = vdupq_n_f32(0.f);

	// Pre-calculate inverse of r2_vec for division approximation
	float32x4_t inv_r2_vec = vrecpeq_f32(r2_vec);
	// Refine reciprocal estimate for better precision (optional, but often good practice for division)
	inv_r2_vec = vmulq_f32(vrecpsq_f32(r2_vec, inv_r2_vec), inv_r2_vec);
	// You can repeat the refinement for even higher precision if needed:
	// inv_r2_vec = vmulq_f32(vrecpsq_f32(r2_vec, inv_r2_vec), inv_r2_vec);

	std::vector<SDL_FPoint> points_;
	points_.reserve(static_cast<size_t>(_r * _r * 4)); // Pre-allocate to reduce reallocations

	float prev_bias = 1.f; // Scalar prev_bias

	// Process y values
	for (float y_scalar = 0.5f; y_scalar <= _r; y_scalar += 1.f)
	{
		// Broadcast current y_scalar to a NEON vector
		float32x4_t y_vec = vdupq_n_f32(y_scalar);
		float32x4_t y2_vec = vmulq_f32(y_vec, y_vec); // y*y

		// Process x values in steps of 4
		for (float x_start = 0.5f; x_start <= _r; x_start += 4.f)
		{
			// Load x values into a NEON vector: [x, x+1, x+2, x+3]
			// Note: x_start + 3.f might exceed _r in the last iteration,
			// but the mask will handle it.
			float32x4_t x_vec = {x_start, x_start + 1.f, x_start + 2.f, x_start + 3.f};
			float32x4_t x2_vec = vmulq_f32(x_vec, x_vec); // x*x

			// Calculate res = x*x + y*y for 4 elements
			float32x4_t res_vec = vaddq_f32(x2_vec, y2_vec);

			// Compare res <= r2_
			uint32x4_t mask = vcleq_f32(res_vec, r2_vec); // mask will have all bits set (0xFFFFFFFF) for true, 0 for false

			// Calculate bias = std::clamp(((1.f - (res / r2_)) * r_4), 0.f, 1.f) for 4 elements
			// Use reciprocal multiplication for division
			float32x4_t res_div_r2_vec = vmulq_f32(res_vec, inv_r2_vec);
			float32x4_t one_minus_res_div_r2_vec = vsubq_f32(one_f_vec, res_div_r2_vec);
			float32x4_t bias_vec = vmulq_f32(one_minus_res_div_r2_vec, r_4_vec);

			// Clamp bias_vec between 0.f and 1.f
			bias_vec = vmaxq_f32(bias_vec, zero_f_vec); // clamp lower bound (max(bias, 0))
			bias_vec = vminq_f32(bias_vec, one_f_vec);	// clamp upper bound (min(bias, 1))

			// Manual unrolling for processing each lane due to `vgetq_lane` limitations
			// and complex scalar logic.
			// This is the bottleneck for full vectorization.
			// Process lane 0
			if (vgetq_lane_u32(mask, 0) != 0) // if true
			{
				float current_x = vgetq_lane_f32(x_vec, 0);
				// Adjust x for points_ if x_start + 0 was beyond _r (though mask handles it)
				if (current_x > _r)
					current_x = _r; // Clamp for correct point position if needed

				float current_y = y_scalar;
				float current_bias = vgetq_lane_f32(bias_vec, 0);

				points_.emplace_back(SDL_FPoint{(_x - current_x), (_y - current_y)});
				points_.emplace_back(SDL_FPoint{(_x + current_x), (_y - current_y)});
				points_.emplace_back(SDL_FPoint{(_x - current_x), (_y + current_y)});
				points_.emplace_back(SDL_FPoint{(_x + current_x), (_y + current_y)});
				if (current_bias != prev_bias)
				{
					const uint8_t alpha_ = static_cast<uint8_t>(prev_bias * static_cast<float>(_color.a));
					SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
					if (!points_.empty())
					{
						RenderPoints(_renderer, points_.data(), points_.size() - 4);
					}
					points_.clear();
					points_.emplace_back(SDL_FPoint{(_x - current_x), (_y - current_y)});
					points_.emplace_back(SDL_FPoint{(_x + current_x), (_y - current_y)});
					points_.emplace_back(SDL_FPoint{(_x - current_x), (_y + current_y)});
					points_.emplace_back(SDL_FPoint{(_x + current_x), (_y + current_y)});
				}
				prev_bias = current_bias;
			}

			// Process lane 1
			if (x_start + 1.f <= _r && vgetq_lane_u32(mask, 1) != 0)
			{
				float current_x = vgetq_lane_f32(x_vec, 1);
				float current_y = y_scalar;
				float current_bias = vgetq_lane_f32(bias_vec, 1);

				points_.emplace_back(SDL_FPoint{(_x - current_x), (_y - current_y)});
				points_.emplace_back(SDL_FPoint{(_x + current_x), (_y - current_y)});
				points_.emplace_back(SDL_FPoint{(_x - current_x), (_y + current_y)});
				points_.emplace_back(SDL_FPoint{(_x + current_x), (_y + current_y)});
				if (current_bias != prev_bias)
				{
					const uint8_t alpha_ = static_cast<uint8_t>(prev_bias * static_cast<float>(_color.a));
					SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
					if (!points_.empty())
					{
						RenderPoints(_renderer, points_.data(), points_.size() - 4);
					}
					points_.clear();
					points_.emplace_back(SDL_FPoint{(_x - current_x), (_y - current_y)});
					points_.emplace_back(SDL_FPoint{(_x + current_x), (_y - current_y)});
					points_.emplace_back(SDL_FPoint{(_x - current_x), (_y + current_y)});
					points_.emplace_back(SDL_FPoint{(_x + current_x), (_y + current_y)});
				}
				prev_bias = current_bias;
			}

			// Process lane 2
			if (x_start + 2.f <= _r && vgetq_lane_u32(mask, 2) != 0)
			{
				float current_x = vgetq_lane_f32(x_vec, 2);
				float current_y = y_scalar;
				float current_bias = vgetq_lane_f32(bias_vec, 2);

				points_.emplace_back(SDL_FPoint{(_x - current_x), (_y - current_y)});
				points_.emplace_back(SDL_FPoint{(_x + current_x), (_y - current_y)});
				points_.emplace_back(SDL_FPoint{(_x - current_x), (_y + current_y)});
				points_.emplace_back(SDL_FPoint{(_x + current_x), (_y + current_y)});
				if (current_bias != prev_bias)
				{
					const uint8_t alpha_ = static_cast<uint8_t>(prev_bias * static_cast<float>(_color.a));
					SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
					if (!points_.empty())
					{
						RenderPoints(_renderer, points_.data(), points_.size() - 4);
					}
					points_.clear();
					points_.emplace_back(SDL_FPoint{(_x - current_x), (_y - current_y)});
					points_.emplace_back(SDL_FPoint{(_x + current_x), (_y - current_y)});
					points_.emplace_back(SDL_FPoint{(_x - current_x), (_y + current_y)});
					points_.emplace_back(SDL_FPoint{(_x + current_x), (_y + current_y)});
				}
				prev_bias = current_bias;
			}

			// Process lane 3
			if (x_start + 3.f <= _r && vgetq_lane_u32(mask, 3) != 0)
			{
				float current_x = vgetq_lane_f32(x_vec, 3);
				float current_y = y_scalar;
				float current_bias = vgetq_lane_f32(bias_vec, 3);

				points_.emplace_back(SDL_FPoint{(_x - current_x), (_y - current_y)});
				points_.emplace_back(SDL_FPoint{(_x + current_x), (_y - current_y)});
				points_.emplace_back(SDL_FPoint{(_x - current_x), (_y + current_y)});
				points_.emplace_back(SDL_FPoint{(_x + current_x), (_y + current_y)});
				if (current_bias != prev_bias)
				{
					const uint8_t alpha_ = static_cast<uint8_t>(prev_bias * static_cast<float>(_color.a));
					SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
					if (!points_.empty())
					{
						RenderPoints(_renderer, points_.data(), points_.size() - 4);
					}
					points_.clear();
					points_.emplace_back(SDL_FPoint{(_x - current_x), (_y - current_y)});
					points_.emplace_back(SDL_FPoint{(_x + current_x), (_y - current_y)});
					points_.emplace_back(SDL_FPoint{(_x - current_x), (_y + current_y)});
					points_.emplace_back(SDL_FPoint{(_x + current_x), (_y + current_y)});
				}
				prev_bias = current_bias;
			}
		}
	}
	// Render any remaining points after the loops
	if (!points_.empty())
	{
		const uint8_t alpha_ = static_cast<uint8_t>(prev_bias * static_cast<float>(_color.a));
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, alpha_);
		RenderPoints(_renderer, points_.data(), points_.size());
	}
}*/

void draw_filled_circle2(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
						 const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
{
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
					RenderPoints(_renderer, points_.data(), points_.size() - 4);
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

void draw_filled_circle(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
						const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
{
	float bias = 1.f;
	float res = 1.f;
	const float r2_ = _r * _r;
	const float r_4 = _r / 2.f;
	std::unordered_map<uint8_t, std::vector<SDL_FPoint>> fp{};
	for (float y = 0.5f; y <= _r; y += 1.f)
	{
		for (float x = 0.5f; x <= _r; x += 1.f)
		{
			res = x * x + y * y;
			if (res <= r2_)
			{
				bias = std::clamp(((1.f - (res / r2_)) * r_4), 0.f, 1.f);
				const uint8_t alpha_ = static_cast<uint8_t>(bias * static_cast<float>(_color.a));
				if (alpha_ > 0)
				{
					auto &points_ = fp[alpha_];
					points_.emplace_back(SDL_FPoint{(_x - x), (_y - y)});
					points_.emplace_back(SDL_FPoint{(_x + x), (_y - y)});
					points_.emplace_back(SDL_FPoint{(_x - x), (_y + y)});
					points_.emplace_back(SDL_FPoint{(_x + x), (_y + y)});
				}
			}
		}
	}
	for (auto &[key, val] : fp)
	{
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, key);
		RenderPoints(_renderer, val.data(), val.size());
	}
}

void draw_filled_circle_4quad(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_w, const float &_h, const float &_r,
							  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
{
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
					RenderPoints(_renderer, points_.data(), points_.size() - 4);
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
	if (!points_.empty())
	{
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, static_cast<uint8_t>(bias));
		RenderPoints(_renderer, points_.data(), points_.size());
	}
}

void draw_filled_circle_4quad2(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_w, const float &_h, const float &_r,
							   const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
{
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
					RenderPoints(_renderer, points_.data(), points_.size() - 4);
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
}

void draw_circle(SDL_Renderer *_renderer, const float &_x, const float &_y, const float &_r,
				 const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
{
	draw_ring(_renderer, _x, _y, _r - 1.f, _r + 1.f, _color);
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
	if (_quadrant == QUADRANT::TOP_LEFT)
		draw_filled_topleft_quadrant(_renderer, _x, _y, _r, _color);
	else if (_quadrant == QUADRANT::TOP_RIGHT)
		draw_filled_topright_quadrant(_renderer, _x, _y, _r, _color);
	else if (_quadrant == QUADRANT::BOTTOM_LEFT)
		draw_filled_bottomleft_quadrant(_renderer, _x, _y, _r, _color);
	else if (_quadrant == QUADRANT::BOTTOM_RIGHT)
		draw_filled_bottomright_quadrant(_renderer, _x, _y, _r, _color);
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
	if (_quadrant == QUADRANT::TOP_LEFT)
		draw_topleft_quadrant(_renderer, _x, _y, _r, _color);
	else if (_quadrant == QUADRANT::TOP_RIGHT)
		draw_topright_quadrant(_renderer, _x, _y, _r, _color);
	else if (_quadrant == QUADRANT::BOTTOM_LEFT)
		draw_bottomleft_quadrant(_renderer, _x, _y, _r, _color);
	else if (_quadrant == QUADRANT::BOTTOM_RIGHT)
		draw_bottomright_quadrant(_renderer, _x, _y, _r, _color);
}

void fillRoundedRectF3(SDL_Renderer *_renderer, SDL_FRect _dest, float _rad,
					   const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
{
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

	SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);
	RenderFillRectsF(_renderer, rects_.data(), rects_.size());
	draw_filled_circle_4quad(_renderer, _dest.x + final_rad, _dest.y + final_rad, _dest.w - (final_rad * 2.f), _dest.h - (final_rad * 2.f), final_rad, _color);
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
	RenderLine(_renderer, _dest.x + final_rad, _dest.y, _dest.x + _dest.w - final_rad, _dest.y);
	RenderLine(_renderer, _dest.x + final_rad, _dest.y + _dest.h, _dest.x + _dest.w - final_rad, _dest.y + _dest.h);
	RenderLine(_renderer, _dest.x + _dest.w, _dest.y + final_rad, _dest.x + _dest.w, _dest.y + _dest.h - final_rad);
	RenderLine(_renderer, _dest.x, _dest.y + final_rad, _dest.x, _dest.y + _dest.h - final_rad);

	draw_quadrant(_renderer, _dest.x + final_rad + 1.f, _dest.y + final_rad + 1.f, final_rad, QUADRANT::TOP_LEFT, _color);
	draw_quadrant(_renderer, _dest.x + _dest.w - final_rad, _dest.y + final_rad + 1.f, final_rad, QUADRANT::TOP_RIGHT, _color);
	draw_quadrant(_renderer, _dest.x + final_rad + 1.f, _dest.y + _dest.h - final_rad, final_rad, QUADRANT::BOTTOM_LEFT, _color);
	draw_quadrant(_renderer, _dest.x + _dest.w - final_rad, _dest.y + _dest.h - final_rad, final_rad, QUADRANT::BOTTOM_RIGHT, _color);
}

void fillRoundedRectOutline2(SDL_Renderer *_renderer, const SDL_FRect &rect, const float &_r, const float &_outline_psz, const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff})
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
	RenderFillRectsF(_renderer, side_rects.data(), side_rects.size());
	draw_ring_4quad(_renderer, _rect.x + final_rad - 1.f, _rect.y + final_rad, side_rects[0].w, side_rects[3].h, final_rad - outline_sz_, final_rad, _color);
}

void DrawCircle(SDL_Renderer *renderer, float x, float y, float r,
				SDL_Color color = {0xff, 0xff, 0xff, 0xff})
{
	draw_circle(renderer, x, y, r, color);
}

void FillCircle(SDL_Renderer *renderer, float x, float y, float r,
				SDL_Color color = {0xff, 0xff, 0xff, 0xff}, Uint8 quadrant = 0)
{
	draw_filled_circle(renderer, x, y, r, color);
}

/*
// Forward declaration for the corner drawing function
static void DrawAAStrokedCornersSymmetricallyF(SDL_Renderer* renderer,
											   float rect_x, float rect_y, float rect_w, float rect_h,
											   float outer_radius_px, float inner_radius_px,
											   const SDL_Color& color) noexcept;

// Helper: Calculates coverage factor (0 to 1) based on signed distance to edge.
// dist_to_edge: positive outside, negative inside.
static float CalculatePixelCoverage(float dist_to_edge) noexcept {
	if (dist_to_edge <= -0.5f) { // Pixel center is well inside the shape
		return 1.0f;
	}
	if (dist_to_edge >= 0.5f) {  // Pixel center is well outside the shape
		return 0.0f;
	}
	// Pixel center is on the anti-aliased boundary
	return 0.5f - dist_to_edge;
}

// Draws all four anti-aliased STROKED corners symmetrically.
static void DrawAAStrokedCornersSymmetricallyF(SDL_Renderer* renderer,
											   float rect_x, float rect_y, float rect_w, float rect_h,
											   float outer_radius_px, float inner_radius_px, /* Can be 0 or negative if stroke is thick */
/*
											   const SDL_Color& color) noexcept {
	if (outer_radius_px <= 0.001f) return; // No radius, no corners to draw

	// Ensure inner_radius is not excessively negative if stroke_width > outer_radius
	// Effectively, if stroke is thicker than outer_radius, inner_radius becomes 0 for coverage calculation.
	float effective_inner_radius_px = std::max(0.0f, inner_radius_px);

	std::vector<SDL_FPoint> alpha_batches[256];
	for (int i = 0; i < 256; ++i) {
		alpha_batches[i].reserve(128); // Pre-allocate
	}

	// Iterate over the canonical top-left quadrant's bounding box (outer_radius_px x outer_radius_px).
	int iteration_limit = static_cast<int>(std::ceil(outer_radius_px));
	for (int qy = 0; qy < iteration_limit; ++qy) {
		for (int qx = 0; qx < iteration_limit; ++qx) {
			float pixel_center_qx = static_cast<float>(qx) + 0.5f;
			float pixel_center_qy = static_cast<float>(qy) + 0.5f;

			// Distance from pixel center to the common center of arcs in the canonical quadrant
			// (which is at outer_radius_px, outer_radius_px)
			float dist_to_common_center = std::sqrt(
				std::pow(pixel_center_qx - outer_radius_px, 2) +
				std::pow(pixel_center_qy - outer_radius_px, 2)
			);

			// SDF relative to the outer arc boundary
			float sdf_outer_arc = dist_to_common_center - outer_radius_px;
			// SDF relative to the inner arc boundary
			float sdf_inner_arc = dist_to_common_center - effective_inner_radius_px;

			float coverage_outer = CalculatePixelCoverage(sdf_outer_arc);
			float coverage_inner = CalculatePixelCoverage(sdf_inner_arc);

			// Stroke coverage is the part covered by outer arc but not by inner arc
			float stroke_coverage_factor = std::max(0.0f, coverage_outer - coverage_inner);

			if (stroke_coverage_factor > 0.001f) {
				Uint8 final_pixel_alpha = static_cast<Uint8>(static_cast<float>(color.a) * stroke_coverage_factor);
				if (final_pixel_alpha > 0) {
					// Add points for all four corners using symmetry
					alpha_batches[final_pixel_alpha].push_back({rect_x + static_cast<float>(qx), rect_y + static_cast<float>(qy)}); // TL
					alpha_batches[final_pixel_alpha].push_back({rect_x + rect_w - 1.0f - static_cast<float>(qx), rect_y + static_cast<float>(qy)}); // TR
					alpha_batches[final_pixel_alpha].push_back({rect_x + static_cast<float>(qx), rect_y + rect_h - 1.0f - static_cast<float>(qy)}); // BL
					alpha_batches[final_pixel_alpha].push_back({rect_x + rect_w - 1.0f - static_cast<float>(qx), rect_y + rect_h - 1.0f - static_cast<float>(qy)}); // BR
				}
			}
		}
	}

	// Draw the batched points
	for (int alpha_val = 1; alpha_val < 256; ++alpha_val) {
		if (!alpha_batches[alpha_val].empty()) {
			SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, static_cast<Uint8>(alpha_val));
			SDL_RenderDrawPointsF(renderer, alpha_batches[alpha_val].data(),
								 static_cast<int>(alpha_batches[alpha_val].size()));
		}
	}
}


// Draws an anti-aliased stroke (outline) of a rounded rectangle.
// _renderer: The SDL renderer.
// _dest: The SDL_FRect defining the outer bounding box of the stroke.
// _rad_percent: Corner radius as a percentage (0-100) of the rectangle's smallest side.
// _stroke_width: Width of the stroke in pixels.
// _color: The stroke color (including alpha).
void strokeRoundedRectF(SDL_Renderer* _renderer, SDL_FRect _dest, float _rad_percent,
						float _stroke_width, const SDL_Color& _color = {0xff, 0xff, 0xff, 0xff}) noexcept {

	if (_dest.w <= 0.001f || _dest.h <= 0.001f || _stroke_width <= 0.001f) {
		return; // Nothing to draw
	}

	_rad_percent = std::max(0.0f, std::min(100.0f, _rad_percent));
	_stroke_width = std::max(0.001f, _stroke_width); // Ensure stroke has some thickness

	float smallest_side_outer = std::min(_dest.w, _dest.h);
	float outer_radius_px = (smallest_side_outer * _rad_percent) / 200.0f;

	outer_radius_px = std::min(outer_radius_px, _dest.w / 2.0f);
	outer_radius_px = std::min(outer_radius_px, _dest.h / 2.0f);
	outer_radius_px = std::max(0.0f, outer_radius_px);

	float inner_radius_px = outer_radius_px - _stroke_width;
	// inner_radius_px can be negative if stroke_width > outer_radius_px.
	// DrawAAStrokedCornersSymmetricallyF will handle this by effectively making the inner hole disappear.

	SDL_BlendMode old_blend_mode;
	SDL_GetRenderDrawBlendMode(_renderer, &old_blend_mode);
	Uint8 old_r, old_g, old_b, old_a;
	SDL_GetRenderDrawColor(_renderer, &old_r, &old_g, &old_b, &old_a);

	SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
	SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a); // Base color for solid parts

	// --- Special case: No radius (sharp corners) ---
	if (outer_radius_px < 0.01f) {
		// Top edge
		SDL_FRect top_edge = {_dest.x, _dest.y, _dest.w, _stroke_width};
		SDL_RenderFillRectF(_renderer, &top_edge);
		// Bottom edge
		SDL_FRect bottom_edge = {_dest.x, _dest.y + _dest.h - _stroke_width, _dest.w, _stroke_width};
		SDL_RenderFillRectF(_renderer, &bottom_edge);
		// Left edge (avoid overdrawing corners of top/bottom)
		SDL_FRect left_edge = {_dest.x, _dest.y + _stroke_width, _stroke_width, _dest.h - 2.0f * _stroke_width};
		if (left_edge.h > 0.0f) SDL_RenderFillRectF(_renderer, &left_edge);
		// Right edge (avoid overdrawing corners of top/bottom)
		SDL_FRect right_edge = {_dest.x + _dest.w - _stroke_width, _dest.y + _stroke_width, _stroke_width, _dest.h - 2.0f * _stroke_width};
		if (right_edge.h > 0.0f) SDL_RenderFillRectF(_renderer, &right_edge);
	} else {
		// --- Draw the four straight segments of the stroke ---
		// Top horizontal segment
		float straight_w = _dest.w - 2.0f * outer_radius_px;
		if (straight_w > 0.0f) {
			SDL_FRect top_stroke = {_dest.x + outer_radius_px, _dest.y, straight_w, _stroke_width};
			SDL_RenderFillRectF(_renderer, &top_stroke);
		}

		// Bottom horizontal segment
		if (straight_w > 0.0f) {
			SDL_FRect bottom_stroke = {_dest.x + outer_radius_px, _dest.y + _dest.h - _stroke_width, straight_w, _stroke_width};
			SDL_RenderFillRectF(_renderer, &bottom_stroke);
		}

		// Left vertical segment
		float straight_h = _dest.h - 2.0f * outer_radius_px;
		if (straight_h > 0.0f) {
			SDL_FRect left_stroke = {_dest.x, _dest.y + outer_radius_px, _stroke_width, straight_h};
			SDL_RenderFillRectF(_renderer, &left_stroke);
		}

		// Right vertical segment
		if (straight_h > 0.0f) {
			SDL_FRect right_stroke = {_dest.x + _dest.w - _stroke_width, _dest.y + outer_radius_px, _stroke_width, straight_h};
			SDL_RenderFillRectF(_renderer, &right_stroke);
		}

		// --- Draw the four anti-aliased corner strokes ---
		DrawAAStrokedCornersSymmetricallyF(_renderer, _dest.x, _dest.y, _dest.w, _dest.h,
										   outer_radius_px, inner_radius_px, _color);
	}

	SDL_SetRenderDrawBlendMode(_renderer, old_blend_mode);
	SDL_SetRenderDrawColor(_renderer, old_r, old_g, old_b, old_a);
}

*/

// Forward declaration for the corner drawing function
static void DrawAAStrokedCornersSymmetricallyF(SDL_Renderer *renderer,
											   float rect_x, float rect_y, float rect_w, float rect_h,
											   float outer_radius_px, float inner_radius_px,
											   const SDL_Color &color) noexcept;

// Helper: Calculates coverage factor (0 to 1) based on signed distance to edge.
// dist_to_edge: positive outside, negative inside.
static float CalculatePixelCoverage(float dist_to_edge) noexcept
{
	if (dist_to_edge <= -0.5f)
	{ // Pixel center is well inside the shape
		return 1.0f;
	}
	if (dist_to_edge >= 0.5f)
	{ // Pixel center is well outside the shape
		return 0.0f;
	}
	// Pixel center is on the anti-aliased boundary
	return 0.5f - dist_to_edge;
}

// Draws all four anti-aliased STROKED corners symmetrically using single vector batching.
static void DrawAAStrokedCornersSymmetricallyF(SDL_Renderer *renderer,
											   float rect_x, float rect_y, float rect_w, float rect_h,
											   float outer_radius_px, float inner_radius_px,
											   const SDL_Color &color) noexcept
{
	if (outer_radius_px <= 0.001f)
		return; // No radius, no corners to draw

	float effective_inner_radius_px = std::max(0.0f, inner_radius_px);
	std::unordered_map<uint8_t, std::vector<SDL_FPoint>> fp{};

	std::vector<SDL_FPoint> point_batch;
	// Reserve some space; average number of points per batch might be small, but total can be large.
	// Max points in one quadrant for a radius R is roughly R*R. Times 4 for symmetry.
	// This is a heuristic.
	point_batch.reserve(static_cast<size_t>(std::ceil(outer_radius_px) * std::ceil(outer_radius_px) * 0.5) * 4);

	Uint8 current_batch_alpha = 0; // Alpha of the points currently in point_batch

	int iteration_limit = static_cast<int>(std::ceil(outer_radius_px));
	for (int qy = 0; qy < iteration_limit; ++qy)
	{
		for (int qx = 0; qx < iteration_limit; ++qx)
		{
			float pixel_center_qx = static_cast<float>(qx) + 0.5f;
			float pixel_center_qy = static_cast<float>(qy) + 0.5f;

			float dist_to_common_center = std::sqrt(
				std::pow(pixel_center_qx - outer_radius_px, 2) +
				std::pow(pixel_center_qy - outer_radius_px, 2));

			float sdf_outer_arc = dist_to_common_center - outer_radius_px;
			float sdf_inner_arc = dist_to_common_center - effective_inner_radius_px;

			float coverage_outer = CalculatePixelCoverage(sdf_outer_arc);
			float coverage_inner = CalculatePixelCoverage(sdf_inner_arc);

			float stroke_coverage_factor = std::max(0.0f, coverage_outer - coverage_inner);
			Uint8 final_pixel_alpha = 0;

			if (stroke_coverage_factor > 0.001f)
			{
				final_pixel_alpha = static_cast<Uint8>(static_cast<float>(color.a) * stroke_coverage_factor);
			}
			if (final_pixel_alpha > 0)
			{
				// If alpha changes and there are points in the batch, draw them
				if (final_pixel_alpha != current_batch_alpha && !point_batch.empty())
				{
					concat_rng(fp[current_batch_alpha], point_batch.begin(), point_batch.end());
					point_batch.clear(); // Prepare for new batch
				}

				current_batch_alpha = final_pixel_alpha; // Update current batch alpha

				//	if (final_pixel_alpha > 0)
				//{
				// Add points for all four corners using symmetry
				point_batch.emplace_back(SDL_FPoint{rect_x + static_cast<float>(qx), rect_y + static_cast<float>(qy)});									// TL
				point_batch.emplace_back(SDL_FPoint{rect_x + rect_w - 1.0f - static_cast<float>(qx), rect_y + static_cast<float>(qy)});					// TR
				point_batch.emplace_back(SDL_FPoint{rect_x + static_cast<float>(qx), rect_y + rect_h - 1.0f - static_cast<float>(qy)});					// BL
				point_batch.emplace_back(SDL_FPoint{rect_x + rect_w - 1.0f - static_cast<float>(qx), rect_y + rect_h - 1.0f - static_cast<float>(qy)}); // BR
			}
		}
	}

	// Draw any remaining points in the last batch
	if (!point_batch.empty() && current_batch_alpha > 0)
	{
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, current_batch_alpha);
		SDL_RenderDrawPointsF(renderer, point_batch.data(), static_cast<int>(point_batch.size()));
	}
	for (auto &[key, val] : fp)
	{
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, key);
		RenderPoints(renderer, val.data(), val.size());
	}
}

// Draws an anti-aliased stroke (outline) of a rounded rectangle.
// _renderer: The SDL renderer.
// _dest: The SDL_FRect defining the outer bounding box of the stroke.
// _rad_percent: Corner radius as a percentage (0-100) of the rectangle's smallest side.
// _stroke_width: Width of the stroke in pixels.
// _color: The stroke color (including alpha).
void fillRoundedRectOutline(SDL_Renderer *_renderer, SDL_FRect _dest, float _rad_percent,
							float _stroke_width, const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
{
	if (_dest.w <= 0.001f || _dest.h <= 0.001f || _stroke_width <= 0.001f)
	{
		return; // Nothing to draw
	}

	_rad_percent = std::max(0.0f, std::min(100.0f, _rad_percent));
	_stroke_width = std::max(0.001f, _stroke_width);

	float smallest_side_outer = std::min(_dest.w, _dest.h);
	float outer_radius_px = (smallest_side_outer * _rad_percent) / 200.0f;

	outer_radius_px = std::min(outer_radius_px, _dest.w / 2.0f);
	outer_radius_px = std::min(outer_radius_px, _dest.h / 2.0f);
	outer_radius_px = std::max(0.0f, outer_radius_px);

	float inner_radius_px = outer_radius_px - _stroke_width;

	SDL_BlendMode old_blend_mode;
	SDL_GetRenderDrawBlendMode(_renderer, &old_blend_mode);
	Uint8 old_r, old_g, old_b, old_a;
	SDL_GetRenderDrawColor(_renderer, &old_r, &old_g, &old_b, &old_a);

	SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);
	// Base color for solid parts will be set with full alpha, corners use calculated alpha.
	SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);

	if (outer_radius_px < 0.01f)
	{ // Special case: No radius (sharp corners)
		std::vector<SDL_FRect> batch{
			// Top edge
			SDL_FRect{_dest.x, _dest.y, _dest.w, _stroke_width},
			// Bottom edge
			SDL_FRect{_dest.x, _dest.y + _dest.h - _stroke_width, _dest.w, _stroke_width},
			// Left edge
			SDL_FRect{_dest.x, _dest.y + _stroke_width, _stroke_width, _dest.h - 2.0f * _stroke_width},
			// Right edge
			SDL_FRect{_dest.x + _dest.w - _stroke_width, _dest.y + _stroke_width, _stroke_width, _dest.h - 2.0f * _stroke_width}};

		RenderFillRectsF(_renderer, batch.data(), batch.size()); // Check height before drawing
	}
	else
	{
		// --- Draw the four straight segments of the stroke (with full original alpha) ---
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);

		float straight_w = _dest.w - 2.0f * outer_radius_px;
		float straight_h = _dest.h - 2.0f * outer_radius_px;
		std::vector<SDL_FRect> batch{
			SDL_FRect{_dest.x + outer_radius_px, _dest.y, straight_w, _stroke_width},
			SDL_FRect{_dest.x + outer_radius_px, _dest.y + _dest.h - _stroke_width, straight_w, _stroke_width},
			SDL_FRect{_dest.x, _dest.y + outer_radius_px, _stroke_width, straight_h},
			SDL_FRect{_dest.x + _dest.w - _stroke_width, _dest.y + outer_radius_px, _stroke_width, straight_h}};
		RenderFillRectsF(_renderer, batch.data(), batch.size()); // Check height before drawing

		// --- Draw the four anti-aliased corner strokes ---
		// The helper function will manage its own SDL_SetRenderDrawColor calls for varying alphas.
		DrawAAStrokedCornersSymmetricallyF(_renderer, _dest.x, _dest.y, _dest.w, _dest.h,
										   outer_radius_px, inner_radius_px, _color);
	}

	SDL_SetRenderDrawBlendMode(_renderer, old_blend_mode);
	SDL_SetRenderDrawColor(_renderer, old_r, old_g, old_b, old_a);
}

// Helper function to calculate anti-aliased alpha.
// dist_to_edge: Distance from pixel center to the ideal circle edge.
//               Negative means inside, positive means outside.
// base_alpha: The original alpha value of the color.
static Uint8 CalculateAlpha(float dist_to_edge, Uint8 base_alpha) noexcept
{
	float alpha_factor = 0.0f;
	// Pixel center is more than 0.5 units inside the edge (fully opaque for this pixel)
	if (dist_to_edge <= -0.5f)
	{
		alpha_factor = 1.0f;
	}
	// Pixel center is between 0.5 units inside and 0.5 units outside (anti-aliased)
	else if (dist_to_edge < 0.5f)
	{										// Note: strictly < 0.5f, not <=
		alpha_factor = 0.5f - dist_to_edge; // Linearly interpolate alpha
	}
	// Pixel center is more than 0.5 units outside the edge (fully transparent for this pixel)
	else
	{
		alpha_factor = 0.0f;
	}
	return static_cast<Uint8>(static_cast<float>(base_alpha) * alpha_factor);
}

static void DrawAACornersSymmetrically_Scanline(SDL_Renderer *renderer,
												float rect_x, float rect_y, float rect_w, float rect_h,
												float radius_px, SDL_Color color) noexcept
{
	if (radius_px <= 0.0f)
		return;

	const int int_radius = static_cast<int>(std::ceil(radius_px));
	const float radius_sq = radius_px * radius_px;

	// Use a cache for alpha values per row to avoid recalculating for symmetric points
	std::vector<Uint8> alpha_cache(int_radius);

	for (int qy = 0; qy < int_radius; ++qy)
	{
		Uint8 last_alpha = 0;
		int run_start_qx = 0;

		for (int qx = 0; qx < int_radius; ++qx)
		{
			const float pixel_center_qy = static_cast<float>(qy) + 0.5f;
			const float dist_y_from_center = pixel_center_qy - radius_px;

			// Check if this pixel can possibly be inside the circle
			if (dist_y_from_center * dist_y_from_center > radius_sq)
			{
				alpha_cache[qx] = 0;
			}
			else
			{
				const float pixel_center_qx = static_cast<float>(qx) + 0.5f;
				const float dist_x_from_center = pixel_center_qx - radius_px;
				const float dist_sq = dist_x_from_center * dist_x_from_center + dist_y_from_center * dist_y_from_center;

				if (dist_sq > radius_sq)
				{
					// Use the optimized sqrt from recommendation #1 for AA band
					const float radius_outer_sq = (radius_px + 0.5f) * (radius_px + 0.5f);
					if (dist_sq < radius_outer_sq)
					{
						alpha_cache[qx] = CalculateAlpha(std::sqrt(dist_sq) - radius_px, color.a);
					}
					else
					{
						alpha_cache[qx] = 0;
					}
				}
				else
				{
					// Inside, but check if it's in the AA band on the inner side
					const float radius_inner_sq = std::max(0.0f, (radius_px - 0.5f) * (radius_px - 0.5f));
					if (dist_sq > radius_inner_sq)
					{
						alpha_cache[qx] = CalculateAlpha(std::sqrt(dist_sq) - radius_px, color.a);
					}
					else
					{
						alpha_cache[qx] = color.a; // Fully opaque
					}
				}
			}

			// If the alpha changes, draw the run of previous pixels
			if (alpha_cache[qx] != last_alpha)
			{
				if (last_alpha > 0)
				{
					SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, last_alpha);
					// Top-left
					SDL_RenderDrawLineF(renderer, rect_x + run_start_qx, rect_y + qy, rect_x + qx - 1, rect_y + qy);
					// Top-right
					SDL_RenderDrawLineF(renderer, rect_x + rect_w - qx, rect_y + qy, rect_x + rect_w - 1 - run_start_qx, rect_y + qy);
					// Bottom-left
					SDL_RenderDrawLineF(renderer, rect_x + run_start_qx, rect_y + rect_h - 1 - qy, rect_x + qx - 1, rect_y + rect_h - 1 - qy);
					// Bottom-right
					SDL_RenderDrawLineF(renderer, rect_x + rect_w - qx, rect_y + rect_h - 1 - qy, rect_x + rect_w - 1 - run_start_qx, rect_y + rect_h - 1 - qy);
				}
				run_start_qx = qx;
				last_alpha = alpha_cache[qx];
			}
		}
		// Draw the last run of the scanline
		if (last_alpha > 0)
		{
			SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, last_alpha);
			int qx = int_radius;
			SDL_RenderDrawLineF(renderer, rect_x + run_start_qx, rect_y + qy, rect_x + qx - 1, rect_y + qy);
			SDL_RenderDrawLineF(renderer, rect_x + rect_w - qx, rect_y + qy, rect_x + rect_w - 1 - run_start_qx, rect_y + qy);
			SDL_RenderDrawLineF(renderer, rect_x + run_start_qx, rect_y + rect_h - 1 - qy, rect_x + qx - 1, rect_y + rect_h - 1 - qy);
			SDL_RenderDrawLineF(renderer, rect_x + rect_w - qx, rect_y + rect_h - 1 - qy, rect_x + rect_w - 1 - run_start_qx, rect_y + rect_h - 1 - qy);
		}
	}
}
// Draws all four anti-aliased rounded corners symmetrically by processing one quadrant
// and mirroring points, batching by alpha. Uses float coordinates.
// renderer: The SDL renderer.
// rect_x, rect_y: Top-left coordinates of the entire rounded rectangle.
// rect_w, rect_h: Width and height of the entire rounded rectangle.
// radius_px: The corner radius in pixels.
// color: The fill color (including base alpha).
static void DrawAACornersSymmetricallyF(SDL_Renderer *renderer,
										float rect_x, float rect_y, float rect_w, float rect_h,
										float radius_px, SDL_Color color) noexcept
{
	if (radius_px <= 0.0f)
		return;
	// std::unordered_map<uint8_t, std::vector<SDL_FPoint>> fp{};
	std::vector<SDL_FPoint> point_batch;
	// point_batch.reserve(static_cast<size_t>(std::ceil(radius_px) * 4)); // Pre-reserve some space
	Uint8 current_batch_alpha = 0;

	// Iterate over the canonical top-left quadrant's bounding box.
	// qx, qy are 0-indexed integer offsets for pixels within this canonical quadrant.
	// Loop up to ceil(radius_px) to cover all pixels potentially affected by the radius.
	for (int qy = 0; qy < static_cast<int>(std::ceil(radius_px)); ++qy)
	{
		for (int qx = 0; qx < static_cast<int>(std::ceil(radius_px)); ++qx)
		{
			// Calculate pixel center in the canonical quadrant's local space
			float pixel_center_qx = static_cast<float>(qx) + 0.5f;
			float pixel_center_qy = static_cast<float>(qy) + 0.5f;

			// Calculate distance from this pixel's center to the canonical circle's center (radius_px, radius_px)
			float dist_x_from_center = pixel_center_qx - radius_px;
			float dist_y_from_center = pixel_center_qy - radius_px;
			float dist_from_center_val = std::sqrt(dist_x_from_center * dist_x_from_center +
												   dist_y_from_center * dist_y_from_center);

			// Distance to the edge of the circle
			float dist_to_edge = dist_from_center_val - radius_px;

			Uint8 pixel_alpha = CalculateAlpha(dist_to_edge, color.a);

			if (pixel_alpha > 0)
			{
				// If the alpha for this pixel is different from the current batch's alpha,
				// draw the old batch and start a new one.
				if (pixel_alpha != current_batch_alpha && !point_batch.empty())
				{
					// concat_rng(fp[current_batch_alpha], point_batch.begin(), point_batch.end());
					SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, current_batch_alpha);
					SDL_RenderDrawPointsF(renderer, point_batch.data(), static_cast<int>(point_batch.size()));
					point_batch.clear();
				}
				current_batch_alpha = pixel_alpha;

				// Add points for all four corners using symmetry.
				// These are the top-left pixels of each corner's quadrant.
				point_batch.emplace_back(SDL_FPoint{rect_x + static_cast<float>(qx), rect_y + static_cast<float>(qy)});									// TL
				point_batch.emplace_back(SDL_FPoint{rect_x + rect_w - 1.0f - static_cast<float>(qx), rect_y + static_cast<float>(qy)});					// TR
				point_batch.emplace_back(SDL_FPoint{rect_x + static_cast<float>(qx), rect_y + rect_h - 1.0f - static_cast<float>(qy)});					// BL
				point_batch.emplace_back(SDL_FPoint{rect_x + rect_w - 1.0f - static_cast<float>(qx), rect_y + rect_h - 1.0f - static_cast<float>(qy)}); // BR
			}
		}
	}

	// Draw any remaining points in the last batch
	if (!point_batch.empty())
	{
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, current_batch_alpha);
		SDL_RenderDrawPointsF(renderer, point_batch.data(), static_cast<int>(point_batch.size()));
	} /*
	for (auto &[key, val] : fp)
	{
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, key);
		RenderPoints(renderer, val.data(), val.size());
	}*/
}

// Draws a filled rectangle with rounded corners, anti-aliased, using float coordinates.
// _renderer: The SDL renderer to draw on.
// _dest: The destination SDL_FRect defining the rectangle's position and size.
// _rad_percent: The corner radius as a percentage (0-100) of the rectangle's smallest side.
//               100% means the radius is half the smallest side.
// _color: The fill color of the rectangle (including alpha).
void fillRoundedRectF(SDL_Renderer *_renderer, SDL_FRect _dest, float _rad_percent,
					  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
{
	// Ensure width and height are positive
	if (_dest.w <= 0.0f || _dest.h <= 0.0f)
	{
		return;
	}

	// Calculate actual radius in pixels
	float smallest_side = std::min(_dest.w, _dest.h);
	float radius_px = (smallest_side * _rad_percent) / 200.0f; // /200 because 100% rad = smallest_side/2

	// Validate and clamp radius_px:
	if (radius_px < 0.0f)
		radius_px = 0.0f;

	float max_radius_possible = std::min(_dest.w / 2.0f, _dest.h / 2.0f);
	if (radius_px > max_radius_possible)
		radius_px = max_radius_possible;

	// Store original renderer state to restore later
	SDL_BlendMode old_blend_mode;
	SDL_GetRenderDrawBlendMode(_renderer, &old_blend_mode);
	Uint8 old_r, old_g, old_b, old_a;
	SDL_GetRenderDrawColor(_renderer, &old_r, &old_g, &old_b, &old_a);

	// Enable alpha blending for anti-aliasing
	SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);

	// If radius is effectively zero, draw a simple, non-rounded rectangle.
	if (radius_px < 0.01f)
	{ // Use a small epsilon for float comparison
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);
		SDL_RenderFillRectF(_renderer, &_dest);
	}
	else
	{
		// --- Draw the opaque central parts of the rounded rectangle ---
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);

		// 1. Central horizontal rectangle
		if (_dest.w - 2.0f * radius_px > 0.0f)
		{
			SDL_FRect center_rect_h = {_dest.x + radius_px, _dest.y, _dest.w - 2.0f * radius_px, _dest.h};
			SDL_RenderFillRectF(_renderer, &center_rect_h);
		}

		// 2. Side vertical strips (to fill the parts of the + shape not covered by center_rect_h)
		if (_dest.h - 2.0f * radius_px > 0.0f)
		{
			// Left strip
			SDL_FRect left_strip_rect = {_dest.x, _dest.y + radius_px, radius_px, _dest.h - 2.0f * radius_px};
			SDL_RenderFillRectF(_renderer, &left_strip_rect);

			// Right strip
			SDL_FRect right_strip_rect = {_dest.x + _dest.w - radius_px, _dest.y + radius_px, radius_px, _dest.h - 2.0f * radius_px};
			SDL_RenderFillRectF(_renderer, &right_strip_rect);
		}

		// --- Draw the anti-aliased corner quadrants ---
		// The DrawAACornersSymmetricallyF function handles all four corners.
		DrawAACornersSymmetricallyF(_renderer, _dest.x, _dest.y, _dest.w, _dest.h, radius_px, _color);
	}

	// Restore original renderer state
	SDL_SetRenderDrawBlendMode(_renderer, old_blend_mode);
	SDL_SetRenderDrawColor(_renderer, old_r, old_g, old_b, old_a);
}

void fillRoundedRectFScanline(SDL_Renderer *_renderer, SDL_FRect _dest, float _rad_percent,
							  const SDL_Color &_color = {0xff, 0xff, 0xff, 0xff}) noexcept
{
	// Ensure width and height are positive
	if (_dest.w <= 0.0f || _dest.h <= 0.0f)
	{
		return;
	}

	// Calculate actual radius in pixels
	float smallest_side = std::min(_dest.w, _dest.h);
	float radius_px = (smallest_side * _rad_percent) / 200.0f; // /200 because 100% rad = smallest_side/2

	// Validate and clamp radius_px:
	if (radius_px < 0.0f)
		radius_px = 0.0f;

	float max_radius_possible = std::min(_dest.w / 2.0f, _dest.h / 2.0f);
	if (radius_px > max_radius_possible)
		radius_px = max_radius_possible;

	// Store original renderer state to restore later
	SDL_BlendMode old_blend_mode;
	SDL_GetRenderDrawBlendMode(_renderer, &old_blend_mode);
	Uint8 old_r, old_g, old_b, old_a;
	SDL_GetRenderDrawColor(_renderer, &old_r, &old_g, &old_b, &old_a);

	// Enable alpha blending for anti-aliasing
	SDL_SetRenderDrawBlendMode(_renderer, SDL_BLENDMODE_BLEND);

	// If radius is effectively zero, draw a simple, non-rounded rectangle.
	if (radius_px < 0.01f)
	{ // Use a small epsilon for float comparison
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);
		SDL_RenderFillRectF(_renderer, &_dest);
	}
	else
	{
		// --- Draw the opaque central parts of the rounded rectangle ---
		SDL_SetRenderDrawColor(_renderer, _color.r, _color.g, _color.b, _color.a);

		// 1. Central horizontal rectangle
		if (_dest.w - 2.0f * radius_px > 0.0f)
		{
			SDL_FRect center_rect_h = {_dest.x + radius_px, _dest.y, _dest.w - 2.0f * radius_px, _dest.h};
			SDL_RenderFillRectF(_renderer, &center_rect_h);
		}

		// 2. Side vertical strips (to fill the parts of the + shape not covered by center_rect_h)
		if (_dest.h - 2.0f * radius_px > 0.0f)
		{
			// Left strip
			SDL_FRect left_strip_rect = {_dest.x, _dest.y + radius_px, radius_px, _dest.h - 2.0f * radius_px};
			SDL_RenderFillRectF(_renderer, &left_strip_rect);

			// Right strip
			SDL_FRect right_strip_rect = {_dest.x + _dest.w - radius_px, _dest.y + radius_px, radius_px, _dest.h - 2.0f * radius_px};
			SDL_RenderFillRectF(_renderer, &right_strip_rect);
		}

		// --- Draw the anti-aliased corner quadrants ---
		// The DrawAACornersSymmetricallyF function handles all four corners.
		DrawAACornersSymmetrically_Scanline(_renderer, _dest.x, _dest.y, _dest.w, _dest.h, radius_px, _color);
	}

	// Restore original renderer state
	SDL_SetRenderDrawBlendMode(_renderer, old_blend_mode);
	SDL_SetRenderDrawColor(_renderer, old_r, old_g, old_b, old_a);
}

// Helper to get a 32-bit pixel value from a surface (assuming ARGB8888 or similar).
// Assumes surface is locked.
static Uint32 GetPixel32FromSurface(SDL_Surface *surface, int x, int y)
{
	Uint32 *pixels = static_cast<Uint32 *>(surface->pixels);
	return pixels[(y * (surface->pitch / 4)) + x];
}

// Helper to put a 32-bit pixel value to a surface (assuming ARGB8888 or similar).
// Assumes surface is locked.
static void PutPixel32ToSurface(SDL_Surface *surface, int x, int y, Uint32 pixel)
{
	Uint32 *pixels = static_cast<Uint32 *>(surface->pixels);
	pixels[(y * (surface->pitch / 4)) + x] = pixel;
}

// Transforms a texture by rounding its corners, making parts outside the rounded shape transparent.
// renderer: The SDL renderer.
// source_texture: The original texture to transform.
// radius_percent: Corner radius as a percentage (0-100) of the texture's smallest side.
// Returns a new SDL_Texture* with rounded corners, or nullptr on failure.
// The caller is responsible for destroying the returned texture.
SDL_Texture *newRoundedTextureFromTexture(SDL_Renderer *renderer, SDL_Texture *source_texture, float radius_percent)
{
	if (!renderer)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "transformToRoundedTexture: Renderer is NULL.");
		return nullptr;
	}
	if (!source_texture)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "transformToRoundedTexture: Source_texture is NULL.");
		return nullptr;
	}

	int w, h;
	Uint32 source_format_enum;
	// Query the source texture for its properties
	if (SDL_QueryTexture(source_texture, &source_format_enum, nullptr, &w, &h) != 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "transformToRoundedTexture: SDL_QueryTexture failed: %s", SDL_GetError());
		return nullptr;
	}

	if (w <= 0 || h <= 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "transformToRoundedTexture: Source_texture has invalid dimensions (w=%d, h=%d).", w, h);
		return nullptr;
	}

	// --- 1. Read source_texture pixels into a CPU-accessible surface in a standard ARGB8888 format ---
	SDL_Surface *source_surface_std_format = nullptr;

	// Create a temporary texture that can be used as a render target to read pixels from.
	// This is a robust way to get pixels regardless of original source_texture's SDL_TEXTUREACCESS.
	SDL_Texture *temp_render_target = SDL_CreateTexture(renderer, source_format_enum, SDL_TEXTUREACCESS_TARGET, w, h);
	if (!temp_render_target)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "transformToRoundedTexture: Failed to create temporary render target texture: %s", SDL_GetError());
		return nullptr;
	}

	SDL_Texture *old_target = SDL_GetRenderTarget(renderer); // Store the current render target
	SDL_SetRenderTarget(renderer, temp_render_target);		 // Set our temporary texture as the render target

	// Ensure source texture is copied as-is, without unexpected blending from its own blend mode.
	SDL_BlendMode old_source_blend_mode;
	SDL_GetTextureBlendMode(source_texture, &old_source_blend_mode);
	SDL_SetTextureBlendMode(source_texture, SDL_BLENDMODE_NONE);

	SDL_RenderCopy(renderer, source_texture, nullptr, nullptr); // Copy the source texture to our temporary target

	SDL_SetTextureBlendMode(source_texture, old_source_blend_mode); // Restore source texture's blend mode

	// Create an SDL_Surface in a known 32-bit format with an alpha channel.
	// SDL_PIXELFORMAT_ARGB8888 is a common choice for such operations.
	source_surface_std_format = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
	if (!source_surface_std_format)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "transformToRoundedTexture: Failed to create ARGB8888 surface for reading pixels: %s", SDL_GetError());
		SDL_DestroyTexture(temp_render_target);
		SDL_SetRenderTarget(renderer, old_target); // Restore original render target
		return nullptr;
	}

	// Read the pixels from the temporary render target into our surface.
	if (SDL_RenderReadPixels(renderer, nullptr, source_surface_std_format->format->format,
							 source_surface_std_format->pixels, source_surface_std_format->pitch) != 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "transformToRoundedTexture: SDL_RenderReadPixels failed: %s", SDL_GetError());
		SDL_FreeSurface(source_surface_std_format);
		SDL_DestroyTexture(temp_render_target);
		SDL_SetRenderTarget(renderer, old_target); // Restore original render target
		return nullptr;
	}

	SDL_DestroyTexture(temp_render_target);	   // Clean up the temporary texture
	SDL_SetRenderTarget(renderer, old_target); // Restore the original render target

	// --- 2. Create target_surface by duplicating source_surface_std_format ---
	// This target_surface will have its alpha channel modified.
	SDL_Surface *target_surface = SDL_DuplicateSurface(source_surface_std_format);
	SDL_FreeSurface(source_surface_std_format); // The original standard format surface is no longer needed

	if (!target_surface)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "transformToRoundedTexture: SDL_DuplicateSurface failed: %s", SDL_GetError());
		return nullptr;
	}
	// Ensure the surface can be blended if it were to be blitted (though we create a texture from it).
	SDL_SetSurfaceBlendMode(target_surface, SDL_BLENDMODE_BLEND);

	// --- 3. Calculate radius in pixels ---
	radius_percent = std::max(0.0f, std::min(100.0f, radius_percent)); // Clamp radius_percent to [0, 100]
	float smallest_side = static_cast<float>(std::min(target_surface->w, target_surface->h));
	float radius_px = (smallest_side * radius_percent) / 200.0f; // /200 because 100% radius = smallest_side/2

	// Clamp radius_px to be valid for the texture dimensions
	radius_px = std::min(radius_px, static_cast<float>(target_surface->w) / 2.0f);
	radius_px = std::min(radius_px, static_cast<float>(target_surface->h) / 2.0f);
	radius_px = std::max(0.0f, radius_px); // Ensure radius is not negative

	// --- 4. Lock target_surface and apply alpha modifications for corners ---
	if (SDL_LockSurface(target_surface) != 0)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "transformToRoundedTexture: SDL_LockSurface failed: %s", SDL_GetError());
		SDL_FreeSurface(target_surface);
		return nullptr;
	}

	SDL_PixelFormat *fmt = target_surface->format; // Get pixel format for SDL_GetRGBA and SDL_MapRGBA

	// Only process corners if the radius is significant. Otherwise, the texture is rectangular.
	// Pixels outside these corner regions will retain their original alpha from the duplication.
	int iteration_radius = static_cast<int>(std::ceil(radius_px));
	if (radius_px > 0.01f)
	{
		for (int qy = 0; qy < iteration_radius; ++qy)
		{ // Iterate y in canonical quadrant
			for (int qx = 0; qx < iteration_radius; ++qx)
			{ // Iterate x in canonical quadrant
				// Calculate center of the current canonical pixel (relative to 0,0 of the quadrant)
				float pixel_center_qx = static_cast<float>(qx) + 0.5f;
				float pixel_center_qy = static_cast<float>(qy) + 0.5f;

				// Distance from this pixel's center to the canonical circle's center (which is at radius_px, radius_px)
				float dist_to_arc_center = std::sqrt(
					std::pow(pixel_center_qx - radius_px, 2) +
					std::pow(pixel_center_qy - radius_px, 2));

				// Signed distance to the edge of the rounding circle for this canonical pixel.
				// Negative if inside, positive if outside.
				float sdf_to_arc_edge = dist_to_arc_center - radius_px;

				// aa_mask_factor determines how much of the original alpha to keep.
				// 1.0 means fully keep (pixel is inside the curve), 0.0 means fully transparent (pixel is outside).
				// Values between 0 and 1 create the anti-aliased edge.
				float aa_mask_factor = CalculatePixelCoverage(sdf_to_arc_edge);

				// Define the four symmetric points on the target surface
				// Top-Left, Top-Right, Bottom-Left, Bottom-Right
				int points_x[4] = {qx, target_surface->w - 1 - qx, qx, target_surface->w - 1 - qx};
				int points_y[4] = {qy, qy, target_surface->h - 1 - qy, target_surface->h - 1 - qy};

				for (int i = 0; i < 4; ++i)
				{
					int actual_x = points_x[i];
					int actual_y = points_y[i];

					// Ensure the symmetric point is within the surface bounds
					if (actual_x >= 0 && actual_x < target_surface->w && actual_y >= 0 && actual_y < target_surface->h)
					{
						// Get the original pixel value (color and alpha) from the duplicated surface
						Uint32 original_pixel_value = GetPixel32FromSurface(target_surface, actual_x, actual_y);
						Uint8 r_orig, g_orig, b_orig, a_orig;
						SDL_GetRGBA(original_pixel_value, fmt, &r_orig, &g_orig, &b_orig, &a_orig);

						// Modulate the original alpha with our mask factor
						Uint8 new_alpha = static_cast<Uint8>(static_cast<float>(a_orig) * aa_mask_factor);

						// Write the pixel back with original color but new alpha
						PutPixel32ToSurface(target_surface, actual_x, actual_y, SDL_MapRGBA(fmt, r_orig, g_orig, b_orig, new_alpha));
					}
				}
			}
		}
	}
	// If radius_px <= 0.01f, no corner processing is done, and the target_surface (a full copy) is used directly.

	SDL_UnlockSurface(target_surface);

	// --- 5. Create the final SDL_Texture from the modified target_surface ---
	SDL_Texture *final_texture = SDL_CreateTextureFromSurface(renderer, target_surface);
	if (!final_texture)
	{
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "transformToRoundedTexture: SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
		// target_surface will be freed below regardless
	}
	else
	{
		// Set blend mode for the new texture to allow its alpha channel to be used correctly
		SDL_SetTextureBlendMode(final_texture, SDL_BLENDMODE_BLEND);
	}

	SDL_FreeSurface(target_surface); // Clean up the target surface

	return final_texture; // Return the new, rounded texture (or nullptr on error)
}

// Modifies a texture in-place by rounding its corners, making parts outside the rounded shape transparent.
// renderer: The SDL renderer
// source_texture: The texture to modify. Must be STREAMING (with alpha channel) or TARGET.
// radius_percent: Corner radius as a percentage (0-100) of the texture's smallest side
void transformToRoundedTexture(SDL_Renderer *renderer, SDL_Texture *source_texture, float radius_percent)
{
	if (!renderer)
	{
		SDL_Log("transformToRoundedTextureInPlace: Renderer is NULL.");
		return;
	}
	if (!source_texture)
	{
		SDL_Log("transformToRoundedTextureInPlace: Source_texture is NULL.");
		return;
	}

	int w, h;
	Uint32 source_format_enum;
	int source_access;
	if (SDL_QueryTexture(source_texture, &source_format_enum, &source_access, &w, &h) != 0)
	{
		SDL_Log("transformToRoundedTextureInPlace: SDL_QueryTexture failed: %s", SDL_GetError());
		return;
	}

	if (w <= 0 || h <= 0)
	{
		SDL_Log("transformToRoundedTextureInPlace: Source_texture has invalid dimensions (w=%d, h=%d).", w, h);
		return;
	}

	// Calculate and clamp radius
	radius_percent = std::max(0.0f, std::min(100.0f, radius_percent));
	float smallest_side = static_cast<float>(std::min(w, h));
	float radius_px = (smallest_side * radius_percent) / 200.0f;
	radius_px = std::min(radius_px, static_cast<float>(w) / 2.0f);
	radius_px = std::min(radius_px, static_cast<float>(h) / 2.0f);
	radius_px = std::max(0.0f, radius_px);

	// Pixel format for intermediate surface if needed (Target Path)
	// User requested to assume RGBA8888 as default
	Uint32 intermediate_pixel_format_enum = SDL_PIXELFORMAT_RGBA8888;
	SDL_PixelFormat *intermediate_format_details = SDL_AllocFormat(intermediate_pixel_format_enum);
	if (!intermediate_format_details)
	{
		SDL_Log("transformToRoundedTextureInPlace: Failed to allocate pixel format %s: %s",
				SDL_GetPixelFormatName(intermediate_pixel_format_enum), SDL_GetError());
		return;
	}

	bool success = false;

	if (source_access == SDL_TEXTUREACCESS_STREAMING)
	{
		// SDL_Log("transformToRoundedTextureInPlace: Processing STREAMING texture.");
		void *pixels_ptr; // Changed name to avoid conflict
		int pitch;

		SDL_PixelFormat *streaming_texture_format_details = SDL_AllocFormat(source_format_enum);
		if (!streaming_texture_format_details)
		{
			SDL_Log("transformToRoundedTextureInPlace (Streaming): Failed to allocate pixel format for source texture (format %s): %s",
					SDL_GetPixelFormatName(source_format_enum), SDL_GetError());
			SDL_FreeFormat(intermediate_format_details);
			return;
		}

		// Simplified check: For direct manipulation, we expect a 32-bit format.
		if (streaming_texture_format_details->BytesPerPixel != 4)
		{
			SDL_Log("transformToRoundedTextureInPlace (Streaming): Texture format (BytesPerPixel=%d) is not 4. This example requires a 32-bit format for direct pixel manipulation.", streaming_texture_format_details->BytesPerPixel);
			SDL_FreeFormat(streaming_texture_format_details);
			SDL_FreeFormat(intermediate_format_details);
			return;
		}

		if (SDL_LockTexture(source_texture, nullptr, &pixels_ptr, &pitch) != 0)
		{
			SDL_Log("transformToRoundedTextureInPlace (Streaming): SDL_LockTexture failed: %s", SDL_GetError());
		}
		else
		{
			int iteration_radius = static_cast<int>(std::ceil(radius_px));
			if (radius_px > 0.01f)
			{ // Only process if radius is significant
				for (int qy = 0; qy < iteration_radius; ++qy)
				{
					for (int qx = 0; qx < iteration_radius; ++qx)
					{
						float pixel_center_qx = static_cast<float>(qx) + 0.5f;
						float pixel_center_qy = static_cast<float>(qy) + 0.5f;
						float dist_to_arc_center = std::sqrt(std::pow(pixel_center_qx - radius_px, 2) + std::pow(pixel_center_qy - radius_px, 2));
						float sdf_to_arc_edge = dist_to_arc_center - radius_px;
						float aa_mask_factor = CalculatePixelCoverage(sdf_to_arc_edge);

						int points_x[4] = {qx, w - 1 - qx, qx, w - 1 - qx};
						int points_y[4] = {qy, qy, h - 1 - qy, h - 1 - qy};

						for (int i = 0; i < 4; ++i)
						{
							int actual_x = points_x[i];
							int actual_y = points_y[i];
							if (actual_x >= 0 && actual_x < w && actual_y >= 0 && actual_y < h)
							{
								Uint32 *target_pixel_ptr = reinterpret_cast<Uint32 *>(static_cast<Uint8 *>(pixels_ptr) + actual_y * pitch + actual_x * streaming_texture_format_details->BytesPerPixel);
								Uint32 original_pixel_value = *target_pixel_ptr;
								Uint8 r_orig, g_orig, b_orig, a_orig;
								SDL_GetRGBA(original_pixel_value, streaming_texture_format_details, &r_orig, &g_orig, &b_orig, &a_orig);
								Uint8 new_alpha = static_cast<Uint8>(static_cast<float>(a_orig) * aa_mask_factor);
								*target_pixel_ptr = SDL_MapRGBA(streaming_texture_format_details, r_orig, g_orig, b_orig, new_alpha);
							}
						}
					}
				}
			}
			SDL_UnlockTexture(source_texture);
			success = true;
		}
		SDL_FreeFormat(streaming_texture_format_details);
	}
	else if (source_access == SDL_TEXTUREACCESS_TARGET)
	{
		// SDL_Log("transformToRoundedTextureInPlace: Processing TARGET texture.");
		SDL_Surface *working_surface = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, intermediate_pixel_format_enum);
		if (!working_surface)
		{
			SDL_Log("transformToRoundedTextureInPlace (Target): Failed to create working surface (format %s): %s",
					SDL_GetPixelFormatName(intermediate_pixel_format_enum), SDL_GetError());
		}
		else
		{
			SDL_Texture *old_rt = SDL_GetRenderTarget(renderer); // Store current render target

			// Temporarily set source_texture as render target to read its current pixels
			// This is a bit of a workaround if source_texture wasn't already the target.
			// A more direct way if source_texture is ALREADY the target would be to just read.
			// For safety, we copy source_texture to a temporary readable target.
			SDL_Texture *temp_readable_target = SDL_CreateTexture(renderer, source_format_enum, SDL_TEXTUREACCESS_TARGET, w, h);
			if (!temp_readable_target)
			{
				SDL_Log("transformToRoundedTextureInPlace (Target): Failed to create temp readable target: %s", SDL_GetError());
				SDL_FreeSurface(working_surface);
				// SDL_FreeFormat(intermediate_format_details) done at the end
			}
			else
			{
				SDL_SetRenderTarget(renderer, temp_readable_target);
				SDL_BlendMode old_source_blend_mode; // Store source's blend mode
				SDL_GetTextureBlendMode(source_texture, &old_source_blend_mode);
				SDL_SetTextureBlendMode(source_texture, SDL_BLENDMODE_NONE);	// Use NONE for direct copy
				SDL_RenderCopy(renderer, source_texture, nullptr, nullptr);		// Copy source to temp_readable_target
				SDL_SetTextureBlendMode(source_texture, old_source_blend_mode); // Restore source's blend mode

				if (SDL_RenderReadPixels(renderer, nullptr, intermediate_pixel_format_enum, working_surface->pixels, working_surface->pitch) != 0)
				{
					SDL_Log("transformToRoundedTextureInPlace (Target): SDL_RenderReadPixels failed: %s", SDL_GetError());
				}
				else
				{
					if (SDL_LockSurface(working_surface) != 0)
					{
						SDL_Log("transformToRoundedTextureInPlace (Target): SDL_LockSurface failed: %s", SDL_GetError());
					}
					else
					{
						int iteration_radius = static_cast<int>(std::ceil(radius_px));
						if (radius_px > 0.01f)
						{ // Only process if radius is significant
							for (int qy = 0; qy < iteration_radius; ++qy)
							{
								for (int qx = 0; qx < iteration_radius; ++qx)
								{
									float pixel_center_qx = static_cast<float>(qx) + 0.5f;
									float pixel_center_qy = static_cast<float>(qy) + 0.5f;
									float dist_to_arc_center = std::sqrt(std::pow(pixel_center_qx - radius_px, 2) + std::pow(pixel_center_qy - radius_px, 2));
									float sdf_to_arc_edge = dist_to_arc_center - radius_px;
									float aa_mask_factor = CalculatePixelCoverage(sdf_to_arc_edge);

									int points_x[4] = {qx, w - 1 - qx, qx, w - 1 - qx};
									int points_y[4] = {qy, qy, h - 1 - qy, h - 1 - qy};
									for (int i = 0; i < 4; ++i)
									{
										int actual_x = points_x[i];
										int actual_y = points_y[i];
										if (actual_x >= 0 && actual_x < w && actual_y >= 0 && actual_y < h)
										{
											Uint32 original_pixel_value = GetPixel32FromSurface(working_surface, actual_x, actual_y);
											Uint8 r_orig, g_orig, b_orig, a_orig;
											SDL_GetRGBA(original_pixel_value, intermediate_format_details, &r_orig, &g_orig, &b_orig, &a_orig);
											Uint8 new_alpha = static_cast<Uint8>(static_cast<float>(a_orig) * aa_mask_factor);
											PutPixel32ToSurface(working_surface, actual_x, actual_y, SDL_MapRGBA(intermediate_format_details, r_orig, g_orig, b_orig, new_alpha));
										}
									}
								}
							}
						}
						SDL_UnlockSurface(working_surface);

						SDL_Texture *temp_modified_texture = SDL_CreateTextureFromSurface(renderer, working_surface);
						if (!temp_modified_texture)
						{
							SDL_Log("transformToRoundedTextureInPlace (Target): SDL_CreateTextureFromSurface failed: %s", SDL_GetError());
						}
						else
						{
							SDL_SetRenderTarget(renderer, source_texture);						// Set original texture as target
							SDL_SetTextureBlendMode(temp_modified_texture, SDL_BLENDMODE_NONE); // Overwrite

							// Clear the target texture with transparent black before drawing new content
							SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
							SDL_RenderClear(renderer);

							SDL_RenderCopy(renderer, temp_modified_texture, nullptr, nullptr); // Copy modified to original
							SDL_DestroyTexture(temp_modified_texture);
							success = true;
						}
					}
				}
				SDL_DestroyTexture(temp_readable_target);
			}
			SDL_SetRenderTarget(renderer, old_rt); // Restore original render target
			SDL_FreeSurface(working_surface);
		}
	}
	else
	{ // SDL_TEXTUREACCESS_STATIC
		SDL_Log("transformToRoundedTextureInPlace: Source_texture is STATIC-only. Cannot modify in-place. Texture must be STREAMING or TARGET.");
	}

	if (success)
	{
		// IMPORTANT: Set the blend mode of the (now modified) source_texture
		// so that its new alpha channel is used when it's rendered later.
		SDL_SetTextureBlendMode(source_texture, SDL_BLENDMODE_BLEND);
		//        SDL_Log("transformToRoundedTextureInPlace: Texture modification process completed for %s texture.", (source_access == SDL_TEXTUREACCESS_STREAMING) ? "STREAMING" : "TARGET");
	}
	else
	{
		SDL_Log("transformToRoundedTextureInPlace: Texture modification failed or was not applicable.");
	}
	SDL_FreeFormat(intermediate_format_details);
}

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

class RectOutline : public Context, IView
{
private:
	SDL_FRect inner_rect{};

public:
	using IView::getView;
	using IView::hide;
	using IView::isHidden;
	using IView::show;
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
		if (event->type == EVT_WPSC)
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

	void draw() override
	{
		fillRoundedRectF(renderer, rect, corner_rad, color);
		fillRoundedRectOutline(renderer, rect, corner_rad, outline, outline_color);
	}
};

class Interpolator
{
public:
	Interpolator &start(float initial_velocity, float _adg = -9.8f)
	{
		vO = initial_velocity;
		ADG = _adg;
		MAX_LEN = 100.f * ((vO * vO) / (2.f * std::fabs(ADG)));
		value = 0.f, prev_dy = 0.f;
		update_physics = true;
		tm_start = SDL_GetTicks();
		return *this;
	}

	Interpolator &startWithDistance(float max_length, float _adg = -9.8f)
	{
		ADG = _adg;
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
		if (dy >= MAX_LEN - 1.f)
			update_physics = false;
		return *this;
	}

	Interpolator &stop() noexcept
	{
		reset();
		return *this;
	}

	float getFactor()
	{
		update();
		return value;
	}

	float getStartDistance() const noexcept
	{
		return MAX_LEN;
	}

	bool isRunning() const noexcept
	{
		return update_physics;
	}

	void reset()
	{
		ADG = -9.8f;
		update_physics = false;
		value = 0;
	}

protected:
	uint32_t tm_start = 0;
	bool update_physics = false;
	// acceleration due to gravity
	float ADG = -9.8f;
	float MAX_LEN = 0.f;
	// initial velocity
	float vO = 0.f;
	float dy = 0.f;
	float prev_dy = 0.f;
	float value = 0.f;
	float dy_sum = 0.f;
};

/*
	 * This interpolator generates values that initially have a small difference between them
	 * and then ramps up the difference gradually until it reaches the endpoint.
	 * For example, the generated values between 1 -> 5 with accelerated interpolation could be 1 -> 1.2 -> 1.5 -> 1.9 -> 2.4 -> 3.0 -> 3.6 -> 4.3 -> 5.

class AccelerateInterpolator
{
};

/*
	 * This interpolator generates values that are “slowing down” as you move forward in the list of generated values.
	 * So, the values generated initially have a greater difference between them and the difference
	 * gradually reduces until the endpoint is reached. Therefore, the generated values
	 * between 1 -> 5 could look like 1 -> 1.8 -> 2.5 -> 3.1 -> 3.6 -> 4.0 -> 4.3 -> 4.5 -> 4.6 -> 4.7 -> 4.8 -> 4.9 -> 5.

class DecelerateInterpolator
{
  public:
	DecelerateInterpolator &start(float initial_velocity)
	{
		vo = initial_velocity;
		distance = 100.f * ((vo * vo) / (2 * std::fabs(adg)));
		value = 0.f, prev_dy = 0.f;
		update_physics = true;
		tm_start = SDL_GetTicks();
		return *this;
	}

	DecelerateInterpolator &startWithDistance(float max_distance)
	{
		distance = max_distance / 100.f;
		vo = std::sqrt((2.f * adg) * distance);
		vo *= 2.f;
		distance = max_distance;
		value = 0.f, prev_dy = 0.f;
		update_physics = true;
		tm_start = SDL_GetTicks();
		return *this;
	}

	// call this to update the values
	DecelerateInterpolator &update()
	{
		if (!update_physics)
			return *this;
		float t = (SDL_GetTicks() - tm_start) / 1000.f;
		// t *= 1.5f;

		dy = ((vo * t) + ((0.5f * adg) * (t * t))) * 100.f;
		value = dy - prev_dy;
		prev_dy = dy;
		if (dy >= distance - 1.f)
			update_physics = false;
		return *this;
	}

	DecelerateInterpolator &setIsInterpolatinating(bool interpolating) noexcept
	{
		update_physics = interpolating;
		return *this;
	}

	float getValue() const noexcept
	{
		return value;
	}

	bool isInterpolating() const noexcept
	{
		return update_physics;
	}

  protected:
	uint32_t tm_start = 0;
	// acceleration due to gravity
	float adg = -9.8f;
	float distance = 0.f;
	// initial velocity
	float vo = 0.f;
	float dy = 0.f;
	float prev_dy = 0.f;
	float value = 0.f;
	bool update_physics = false;
};

class AccelerateDecelerateInterpolator
{
};

/*
	 * This interpolation starts by first moving backward, then “flings” forward,
	 * and then proceeds gradually to the end. This gives it an effect similar to
	 * cartoons where the characters pull back before shooting off running.
	 * For example, generated values between 1 -> 3 could look like: 1 -> 0.5 -> 2 -> 2.5 -> 3.

class AnticipateInterpolator
{
};

class BounceInterpolator
{
};

/*
	 * This interpolator generates values uniformly from the start to end.
	 * However, after hitting the end, it overshoots or goes beyond the last
	 * value by a small amount and then comes back to the endpoint.
	 * For example, the generated values between 1 -> 5 could look like: 1 -> 2 -> 3 -> 4 -> 5 -> 5.5 -> 5.

class OvershootInterpolator
{
};

class AnticipateOvershootInterpolator
{
};
*/




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

class ImageButton : public Context, public IView
{
public:
	ImageButton &setContext(Context *_context, IView *parent = nullptr)
	{
		Context::setContext(_context, parent);
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
	ImageButton &onClick(std::function<void()> _onClickedCallBack)
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
		async_load_future_ = std::async(std::launch::async, _customSurfaceLoader);
		adaptiveVsyncHD.startRedrawSession();
		return *this;
	}

	void draw() override
	{
		[[unlikely]] if (build_with_async_)
			checkAndProcessAsyncProgress();
		RenderTexture(renderer, texture_.get(), nullptr, &bounds);
	}

	SDL_FRect &getRect() noexcept
	{
		return getBoundsBox();
	}

	bool isAsyncLoadComplete() const noexcept
	{
		return is_async_load_done();
	}

	bool handleEvent() override
	{
		if (!enabled)
			return false;
		bool result = false;
		if (event->type == EVT_MOUSE_BTN_DOWN)
		{
			if (isPointInBound(event->button.x, event->button.y))
			{
				touch_down_ = true;
				shrinkButton();
				result = true;
			}
		}
		else if (event->type == EVT_FINGER_DOWN)
		{
			if (isPointInBound(event->tfinger.x * DisplayInfo::Get().RenderW, event->tfinger.y * DisplayInfo::Get().RenderH))
			{
				haptics->play_effect();
				touch_down_ = true;
				result = true;
				shrinkButton();
			}
		}
		else if (event->type == EVT_MOUSE_MOTION)
		{
			motion_occured_ = true;
		}
		else if (event->type == EVT_MOUSE_BTN_UP)
		{
			unshrinkButton();
			if (isPointInBound(event->button.x, event->button.y) && touch_down_ /* && !motion_occured_*/)
			{
				if (onClickedCallBack_ != nullptr)
					onClickedCallBack_();
				result = true;
			}
			touch_down_ = false;
			motion_occured_ = false;
		}
		else if (event->type == EVT_FINGER_UP)
		{
			unshrinkButton();
			if (isPointInBound(event->tfinger.x * DisplayInfo::Get().RenderW, event->tfinger.y * DisplayInfo::Get().RenderH) && touch_down_ /* && !motion_occured_*/)
			{
				if (onClickedCallBack_ != nullptr)
					onClickedCallBack_();
				result = true;
			}
			touch_down_ = false;
			motion_occured_ = false;
		}
		else if (event->type == EVT_WPSC)
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

	void updatePosBy(float _dx, float _dy) override
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
		}

		corner_radius_ = _corner_radius;
		bg_color_ = _bg_color;

		CacheRenderTarget cache_r_target(renderer);
		texture_.reset();
		texture_ = CreateSharedTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_TARGET, (int)(bounds.w),
									   (int)(bounds.h));
		SDL_SetTextureBlendMode(texture_.get(), SDL_BLENDMODE_BLEND);
		SDL_SetRenderTarget(renderer, texture_.get());
		RenderClear(renderer, 0, 0, 0, 0);
		fillRoundedRectF(renderer, {0.f, 0.f, bounds.w, bounds.h}, corner_radius_, bg_color_);
		RenderTexture(renderer, _texture, nullptr, &img_rect_);
		cache_r_target.release(renderer);
		transformToRoundedTexture(renderer, texture_.get(), corner_radius_);
		configureShrinkSize();
	}

	bool is_async_load_done() const noexcept
	{
		// potential double check for instances created with async
		// one in the draw call && the other on check_and_process_async
		if (!build_with_async_)
			return true;
		if (async_load_future_.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
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
			adjust_image_rect_to_fit(rw, rh);
			async_free1_ = executor_.enqueue(SDL_DestroySurface, async_res_);
			CacheRenderTarget crt_(renderer);
			SDL_SetRenderTarget(renderer, texture_.get());
			RenderClear(renderer, bg_color_.r, bg_color_.g, bg_color_.b, bg_color_.a);
			RenderTexture(renderer, async_res_tex_, nullptr, &img_rect_);
			crt_.release(renderer);
			transformToRoundedTexture(renderer, texture_.get(), corner_radius_);
			SDL_DestroyTexture(async_res_tex_);
			build_with_async_ = false;
		}
	}

	const bool isPointInBound(float x, float y) const noexcept
	{
		if (x > pv->getRealX() + bounds.x && x < (pv->getRealX() + bounds.x + bounds.w) && y > pv->getRealY() + bounds.y && y < pv->getRealY() + bounds.y + bounds.h)
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
		bounds.x += shrink_size_;
		bounds.y += shrink_size_;
		bounds.w -= shrink_size_ * 2.f;
		bounds.h -= shrink_size_ * 2.f;
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
		return surface;
	}

private:
	uint32_t async_start;
	SharedTexture texture_;
	SDL_FRect img_rect_;
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


struct TextBoxAttributes
{
	Font mem_font = Font::RobotoBold;
	SDL_FRect rect = { 0.f, 0.f, 0.f, 0.f };
	TextAttributes textAttributes = { "", {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff} };
	SDL_FRect margin = { 0.f, 0.f, 0.f, 0.f };
	std::string fontFile;
	FontStyle fontStyle = FontStyle::Normal;
	EdgeType edgeType = EdgeType::RECT;
	uint32_t maxlines = 1;
	TextWrapStyle textWrapStyle = TextWrapStyle::MAX_CHARS_PER_LN;
	Gravity gravity = Gravity::Left;
	float cornerRadius = 0.f;
	int customFontstyle = 0x00;
	float lineSpacing = 0.f;
	float outline = 0.f;
	bool isButton = false;
	bool shrinkToFit = false;
	bool useHaptics = false;
	bool highlightOnHover = false;
	SDL_Color outlineColor = { 0x00, 0x00, 0x00, 0x00 };
	SDL_Color onHoverOutlineColor = { 0x00, 0x00, 0x00, 0x00 };
	SDL_Color onHoverBgColor = { 0x00, 0x00, 0x00, 0x00 };
	SDL_Color onHoverTxtColor = { 0x00, 0x00, 0x00, 0x00 };
};

class TextBox : public Context, public IView
{
public:
	TextBox& setContext(Context* _context)
	{
		Context::setContext(_context);
		Context::setView(this);
		return *this;
	}

	TextBox& Build(Context* _context, const TextBoxAttributes& textboxAttr_)
	{
		Context::setContext(_context);
		config_dat_ = textboxAttr_;
		coner_radius_ = textboxAttr_.cornerRadius;
		line_skip_ = textboxAttr_.lineSpacing;
		isButton = textboxAttr_.isButton;
		bounds = textboxAttr_.rect;
		// outlineRect.Build(this, textboxAttr_.rect, textboxAttr_.outline, textboxAttr_.conerRadius, textboxAttr_.textAttributes.bg_color, textboxAttr_.outlineColor);

		text_rect_ = { to_cust(textboxAttr_.margin.x, bounds.w),
					  to_cust(textboxAttr_.margin.y, bounds.h),
					  to_cust(100.f - (textboxAttr_.margin.w + textboxAttr_.margin.x), bounds.w),
					  to_cust(100.f - (textboxAttr_.margin.h + textboxAttr_.margin.y), bounds.h) };
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

		TTF_Font* font = getFont();
		if (!font)
		{
			GLogger.Log(Logger::Level::Info, "TextBox::getFont Error: Font not available.");
			return *this;
		}
		int m_width_px = 0;
		if (TTF_GlyphMetrics(font, 'a', nullptr, nullptr, nullptr, nullptr, &m_width_px) != 0)
		{
			m_width_px = text_rect_.h / 2; // Fallback if metrics fail
		}
		m_width_px = text_rect_.h / 2;
		if (m_width_px == 0)
			m_width_px = 1; // Avoid division by zero

		max_displayable_chars_per_ln_ = static_cast<uint32_t>(text_rect_.w / m_width_px);
		if (text_attributes_.text.size() < max_displayable_chars_per_ln_)
		{
			wrapped_text_.emplace_back(text_attributes_.text);
		}
		else
		{
			if (text_wrap_style_ == TextWrapStyle::MAX_WORDS_PER_LN)
			{
				TextProcessor::Get().wrap_by_word_unicode(text_attributes_.text, &wrapped_text_, " ,-_?\./:;|}[]())!",
					max_displayable_chars_per_ln_, max_lines_);
			}
			else
			{
				TextProcessor::Get().wrap_max_char_unicode(text_attributes_.text, &wrapped_text_, max_displayable_chars_per_ln_, max_lines_);
			}
		}
		/*if(max_displayable_chars_per_ln_<text_attributes_.text.size()){
				wrapped_text_.back() = wrapped_text_.back().replace(wrapped_text_.back().begin()+ (wrapped_text_.back().size()-3), wrapped_text_.back().end(),"...");
			}*/
		max_displayable_lines_ = std::clamp(static_cast<uint32_t>(std::floorf(bounds.h / text_rect_.h)), (uint32_t)1, (uint32_t)1000000);
		capture_src_ = { 0.f, 0.f, text_rect_.w, (text_rect_.h) * std::clamp((float)wrapped_text_.size(), 1.f, (float)max_displayable_lines_) };
		dest_src_ = text_rect_;
		dest_src_.h = capture_src_.h;
		dest_src_.x += bounds.x;
		dest_src_.y += bounds.y;

		this->texture_.reset();
		// CacheRenderTarget crt_(renderer);
		this->texture_ = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_TARGET, static_cast<int>(text_rect_.w),
			static_cast<int>(text_rect_.h * wrapped_text_.size()));

		outlineRect.Build(this, { textboxAttr_.rect.x, textboxAttr_.rect.y, textboxAttr_.rect.w, textboxAttr_.rect.h /* capture_src_.w, capture_src_.h*/ }, textboxAttr_.outline, textboxAttr_.cornerRadius, textboxAttr_.textAttributes.bg_color, textboxAttr_.outlineColor);

		// SDL_SetTextureBlendMode(this->texture_.get(), SDL_BLENDMODE_BLEND);
		// SDL_SetRenderTarget(renderer, this->texture_.get());
		// RenderClear(renderer, 0, 0, 0, 0);
		// FontSystem::Get().setFontAttributes({ font_attributes_.font_file.c_str(), font_attributes_.font_style, font_attributes_.font_size, 0.f }, custom_fontstyle_);
		try
		{
			genText();
		}
		catch (std::exception e)
		{
			GLogger.Log(Logger::Level::Error, e.what());
		}
		return *this;
	}

public:
	void draw() override
	{
		if (hidden)return;
		/*SDL_SetRenderDrawColor(renderer, text_attributes_.bg_color.r, text_attributes_.bg_color.g, text_attributes_.bg_color.b, text_attributes_.bg_color.a);
			RenderFillRect(renderer, &dest_);*/
		outlineRect.draw();
		// fillRoundedRectF(renderer, dest_, coner_radius_, text_attributes_.bg_color);
		// RenderTexture(renderer, texture_.get(), NULL, &dest_src_);
		RenderTexture(renderer, texture_.get(), &capture_src_, &dest_src_);
		for (auto iview : on_click_views) {
			if (not iview->hidden)
				iview->draw();
		}
		// SDL_Log("d:%f,%f,%f,%f",dest_src_.x,dest_src_.y,dest_src_.w,dest_src_.h);
		// SDL_Log("s:%f,%f,%f,%f",capture_src_.x,capture_src_.y,capture_src_.w,capture_src_.h);
		//  return *this;
	}

	TextBox& onClick(std::function<bool(TextBox*)> _on_clicked_callback) noexcept
	{
		onClickedCallback_ = _on_clicked_callback;
		return *this;
	}

	TextBox& addOnClickView(IView* _on_click_view) noexcept
	{
		on_click_views.push_back(_on_click_view);
		return *this;
	}

	constexpr inline TextBox& setConerRadius(const float& cr_) noexcept
	{
		this->coner_radius_ = cr_;
		return *this;
	}

	TextBox& setId(const uint32_t& _id) noexcept
	{
		this->id_ = _id;
		return *this;
	}

	SDL_FRect& getBounds()
	{
		return bounds;
	}

	float getRealPosX() { return pv->getRealX() + bounds.x; }

	float getRealPosY() { return pv->getRealY() + bounds.y; }

	inline const SDL_FRect& getBoundsConst() const noexcept
	{
		return this->bounds;
	}

	TextBox& setPos(const float x, const float y)
	{
		return *this;
	}

	inline void updatePosBy(float x, float y) override
	{
		update_pos_internal(x, y, false);
		// return *this;
	}

	TextBox& updatePosByAnimated(const float x, const float y)
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

	void setEnabled(const bool& _enabled)
	{
		is_enabled_ = _enabled;
	}

	uint32_t getId() const
	{
		return this->id_;
	}

	TextBox& updateTextColor(const SDL_Color& bgColor, const SDL_Color& outlineColor, const SDL_Color& textColor)
	{
		text_attributes_.bg_color = bgColor;
		text_attributes_.text_color = textColor;
		outlineRect.color = bgColor;
		outlineRect.outline_color = outlineColor;

		CacheRenderTarget crt_(renderer);
		SDL_SetRenderTarget(renderer, this->texture_.get());
		SDL_SetTextureBlendMode(this->texture_.get(), SDL_BLENDMODE_BLEND);
		RenderClear(renderer, 0, 0, 0, 0);
		const SDL_FRect cache_text_rect = text_rect_;
		int tmp_sw = 0, tmp_sh = 0;
		text_rect_.y = 0.f;
		text_rect_.x = 0.f;

		TTF_Font* tmpFont = nullptr;
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
		FontSystem::Get().setFontAttributes({ font_attributes_.font_file.c_str(), font_attributes_.font_style, font_attributes_.font_size }, custom_fontstyle_);
		for (auto const& line_ : wrapped_text_)
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
					if (gravity_ == Gravity::Center)
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
				RenderTexture(renderer, textTex.value().get(), nullptr, &text_rect_);
				text_rect_.y += line_skip_ + text_rect_.h;
			}
			else
				break;
		}
		text_rect_ = cache_text_rect;
		crt_.release(renderer);
		return *this;
	}

	TextBox& updateText(const std::string new_text)
	{
		text_attributes_.text = new_text;
		wrapped_text_.clear();
		if (text_wrap_style_ == TextWrapStyle::MAX_WORDS_PER_LN)
		{
			TextProcessor::Get().wrap_by_word_unicode(
				text_attributes_.text, &wrapped_text_, " ,-_?\./:;|}[]())!",
				max_displayable_chars_per_ln_, max_lines_);
		}
		else
		{
			TextProcessor::Get().wrap_max_char_unicode(
				text_attributes_.text, &wrapped_text_, max_displayable_chars_per_ln_, max_lines_);
		}
		try
		{
			genText();
		}
		catch (std::exception e)
		{
			std::cout << e.what();
		}
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
		for (auto iview : on_click_views) {
			if (not iview->hidden)
				result |= iview->handleEvent();
		}
		if (result) return result;
		/*for (auto iview : on_click_views) {
			iview->hide();
		}*/
		switch (event->type)
		{
		case EVT_RENDER_TARGETS_RESET:
			resolveTextureReset();
			result = true;
			break;
		case EVT_MOUSE_BTN_DOWN:
			if (onClick(event->button.x, event->button.y))
			{
				mouse_in_bound_ = true, result = true;
				if (config_dat_.useHaptics)
					haptics->play_effect();
			}
			else
				mouse_in_bound_ = false;
			break;
		case EVT_MOUSE_MOTION:
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
			 case EVT_MOUSE_BTN_UP:
				 if (onClick(event->motion.x, event->motion.y)) {
					 if (isButton)result = true;
					 if (onClickedCallback_!=nullptr) {
						 result = true;
						 onClickedCallback_(this);
					 }
				 }
				 mouse_in_bound_ = false;
				 break;*/
		case EVT_WPSC:
		case EVT_WMAX:
			config_dat_.rect =
			{
				DisplayInfo::Get().toUpdatedWidth(config_dat_.rect.x),
				DisplayInfo::Get().toUpdatedHeight(config_dat_.rect.y),
				DisplayInfo::Get().toUpdatedWidth(config_dat_.rect.w),
				DisplayInfo::Get().toUpdatedHeight(config_dat_.rect.h),
			};
			this->Build(this, config_dat_);
			break;
		case EVT_FINGER_UP:
			if (onClick(event->tfinger.x * DisplayInfo::Get().RenderW, event->tfinger.y * DisplayInfo::Get().RenderH))
			{
				if (isButton)
					result = true;
				for (auto iview : on_click_views) {
					iview->toggleView();
				}
				if (onClickedCallback_)
				{
					result = onClickedCallback_(this);
				}
			}
			mouse_in_bound_ = false;
			break;
		}
		return result;
	}

private:
	TTF_Font* getFont()
	{
		if (font_attributes_.font_file.empty())
		{
			// SDL_Log("TX: %s,     ws: %d", text_attributes_.text.c_str(),wrapped_text_.size());
			Fonts[config_dat_.mem_font]->font_size = font_attributes_.font_size;
			font_attributes_.font_file = Fonts[config_dat_.mem_font]->font_name;
			return FontSystem::Get().getFont(*Fonts[config_dat_.mem_font]);
		}
		else
		{
			return FontSystem::Get().getFont(font_attributes_.font_file, font_attributes_.font_size);
		}
	}

	void genText()
	{
		CacheRenderTarget crt_(renderer);
		SDL_SetTextureBlendMode(this->texture_.get(), SDL_BLENDMODE_BLEND);
		SDL_SetRenderTarget(renderer, this->texture_.get());
		RenderClear(renderer, 0, 0, 0, 0);
		const SDL_FRect cache_text_rect = text_rect_;
		int tmp_sw = 0, tmp_sh = 0, i = 0;
		text_rect_.y = 0.f;
		text_rect_.x = 0.f;
		TTF_Font* tmpFont = getFont();

		const auto fa_ = static_cast<float>(TTF_FontAscent(tmpFont));
		const auto fd_ = static_cast<float>(TTF_FontDescent(tmpFont));

		FontSystem::Get().setFontAttributes(std::move(FontAttributes{ font_attributes_.font_file.c_str(), font_attributes_.font_style, font_attributes_.font_size }), custom_fontstyle_);
		for (auto const& line_ : wrapped_text_)
		{
			/*
				auto start_ = std::chrono::system_clock::now();
				{
					auto textTex = FontSystem::Get().genTextTextureUnique(renderer, line_.c_str(), this->text_attributes_.text_color);
					std::chrono::duration<double> dt = std::chrono::system_clock::now() - start_;
					std::cout << "otlt:" << dt.count() << std::endl;
				}
				start_= std::chrono::system_clock::now();
				auto textTex = FontSystem::Get().genTextTextureUniqueV2(renderer, line_.c_str(), this->text_attributes_.text_color,text_rect_.w, text_rect_.h,true);
				std::chrono::duration<double> dt = std::chrono::system_clock::now() - start_;
				std::cout << "ntlt:" << dt.count() << std::endl;*/
				// SDL_Log("TXL: %s", line_.c_str());

			auto textTex = FontSystem::Get().genTextTextureUnique(renderer, line_.c_str(), this->text_attributes_.text_color);
			if (textTex.has_value())
			{
				SDL_QueryTexture(textTex.value().get(), nullptr, nullptr, &tmp_sw, &tmp_sh);
				// SDL_Log("TW: %d, TH: %d", tmp_sw, tmp_sh);
				text_rect_.w = static_cast<float>(tmp_sw);
				// std::cout << "TT: " <<tmp_sh<< " - FA: " << fa_ << " - FD: " << fd_ << std::endl;
				text_rect_.h = static_cast<float>(tmp_sh);
				// text_rect_.h = std::clamp(text_rect_.h, 0.f, text_rect_.h);
				text_rect_.w = std::clamp(text_rect_.w, 0.f, cache_text_rect.w);
				if (static_cast<float>(tmp_sh) > text_rect_.h)
					text_rect_.y += fd_, text_rect_.h = static_cast<float>(tmp_sh);

				if (max_lines_ == 1)
				{
					if (gravity_ == Gravity::Center)
					{
						text_rect_.x = ((cache_text_rect.w - text_rect_.w) / 2.f);
					}
					if (gravity_ == Gravity::Right)
					{
						text_rect_.x = ((cache_text_rect.w - text_rect_.w));
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
				RenderTexture(renderer, textTex.value().get(), nullptr, &text_rect_);
				text_rect_.y += line_skip_ + text_rect_.h;
				text_rect_.y += fd_;
			}
			else
				break;
			//++i;
			// if (i >= max_lines_)
			//	break;
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
	template <typename T>
	bool onClick(T x, T y, unsigned short axis = 0)
	{
		[[likely]] if (axis == 0)
		{
			if (x < pv->getRealX() + bounds.x || x >(pv->getRealX() + bounds.x + bounds.w) || y < pv->getRealY() + bounds.y ||
				y >(pv->getRealY() + bounds.y + bounds.h))
				return false;
		}
		else if (axis == 1 /*x-axis only*/)
		{
			if (x < bounds.x || x >(bounds.x + bounds.w))
				return false;
		}
		else if (axis == 2 /*y-axis only*/)
		{
			if (y < bounds.y || y >(bounds.y + bounds.h))
				return false;
		}
		return true;
	}

	inline void update_pos_internal(const float& x, const float& y, const bool& _is_animated) noexcept
	{
		bounds.x += x, bounds.y += y;
		dest_src_.x += x, dest_src_.y += y;
		config_dat_.rect.x += x, config_dat_.rect.y += y;
		outlineRect.rect.x += x, outlineRect.rect.y += y;
	}

protected:
	SDL_FRect text_rect_, dest_src_, capture_src_;
	RectOutline outlineRect;
	SharedTexture texture_;
	ImageButton image_button_;
	std::deque<std::string> wrapped_text_;
	std::function<bool(TextBox*)> onClickedCallback_;
	std::vector<IView*>on_click_views{};
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
};

class RunningText : public Context, public IView
{
public:
	struct Attr
	{
		SDL_FRect rect{ 0.f, 0.f, 0.f, 0.f };
		SDL_Color text_color{ 255, 255, 255, 255 };
		SDL_Color bg_color{ 0, 0, 0, 0 };
		Font mem_font = Font::RobotoBold;
		FontStyle font_style = FontStyle::Normal;
		std::string font_file;
		uint32_t pause_duration = 8000; // 8 seconds
		uint32_t speed = 5000;
		// transition speed in 0.f%-100.f%
		float transition_speed = 2.f;
	};

public:
	RunningText() = default;
	RunningText(Context* _context, const RunningText::Attr& _attr)
	{
		Build(_context, _attr);
	}

	RunningText& Build(Context* _context, const RunningText::Attr& _attr)
	{
		setContext(_context);
		adaptiveVsyncHD.setAdaptiveVsync(adaptiveVsync);
		attr = _attr;
		bounds = _attr.rect;
		attr.transition_speed = DisplayInfo::Get().to_cust(_attr.transition_speed, bounds.h);
		step_tm = (float)attr.speed / bounds.w;
		texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, bounds.w, bounds.h);
		SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
		CacheRenderTarget crt_(renderer);
		SDL_SetRenderTarget(renderer, texture.get());
		RenderClear(renderer, attr.bg_color.r, attr.bg_color.g, attr.bg_color.b, attr.bg_color.a);
		crt_.release(renderer);
		return *this;
	}

	bool handleEvent() override
	{
		bool result = false;
		return result;
	}

	bool isAnimating() const { return is_running; }

	void updatePosBy(float _dx, float _dy) override
	{
		bounds.x += _dx;
		bounds.y += _dy;
	}

	std::string getText() { return text_; }

	void updateText(const std::string& _text)
	{
		text_ = _text;
		if (_text.empty())
			return;
		is_running = false;
		is_centered = false;
		adaptiveVsyncHD.stopRedrawSession();

		CacheRenderTarget crt_(renderer);
		SDL_SetRenderTarget(renderer, texture.get());
		RenderClear(renderer, attr.bg_color.r, attr.bg_color.g, attr.bg_color.b, attr.bg_color.a);

		FontAttributes fontAttrb = { attr.font_file, attr.font_style, static_cast<uint8_t>(bounds.h) };
		TTF_Font* tmpFont = nullptr;
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
		SDL_FRect dst{ 0.f, 0.f, bounds.w, bounds.h };
		dst.w = static_cast<float>(tw);
		if (static_cast<float>(th) > dst.h)
			dst.y += fd_, dst.h = static_cast<float>(th);
		if (dst.w <= bounds.w)
			is_centered = true, dst.x += ((bounds.w - dst.w) / 2.f);
		RenderTexture(renderer, text_texture.get(), nullptr, &dst);
		txt_rect = dst;
		txt_rect2 = dst;
		txt_rect2.x = dst.w + DisplayInfo::Get().to_cust(40.f, bounds.w);
		cache_txt_rect2_x = txt_rect2.x;
		crt_.release(renderer);
		tm_last_pause = SDL_GetTicks();
		tm_last_update = tm_last_pause; //+attr.pause_duration+1;
		Async::GThreadPool.enqueue([this]()
			{SDL_Delay(attr.pause_duration); WakeGui(); });
		// adaptiveVsyncHD.startRedrawSession();
	}

	void draw() override
	{
		if (not is_centered)
		{
			const auto now = SDL_GetTicks();
			if (now - tm_last_pause > attr.pause_duration and not is_running)
			{
				is_running = true;
				tm_last_update = SDL_GetTicks();
				adaptiveVsyncHD.startRedrawSession();
			}
			if (is_running)
				update();
		}
		else
			adaptiveVsyncHD.stopRedrawSession();
		RenderTexture(renderer, texture.get(), nullptr, &bounds);
	}

private:
	void update()
	{
		CacheRenderTarget crt_(renderer);
		SDL_SetRenderTarget(renderer, texture.get());
		RenderClear(renderer, attr.bg_color.r, attr.bg_color.g, attr.bg_color.b, attr.bg_color.a);
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
			Async::GThreadPool.enqueue([this]()
				{SDL_Delay(attr.pause_duration); WakeGui(); });
			adaptiveVsyncHD.stopRedrawSession();
		}
		RenderTexture(renderer, text_texture.get(), nullptr, &txt_rect);
		RenderTexture(renderer, text_texture.get(), nullptr, &txt_rect2);
		crt_.release(renderer);
	}

private:
	std::string text_ = "";
	Attr attr;
	SharedTexture texture;
	SharedTexture text_texture;
	SDL_FRect txt_rect{ 0.f, 0.f, 0.f, 0.f };
	SDL_FRect txt_rect2{ 0.f, 0.f, 0.f, 0.f };
	bool is_running = false;
	bool is_centered = false;
	uint32_t tm_last_pause = 0;
	float cache_txt_rect2_x = 0.f;
	uint32_t step_tm = 0;
	uint32_t tm_last_update = 0;
	AdaptiveVsyncHandler adaptiveVsyncHD;
};

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

	SDL_FRect getRect() const
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
		if (diff_ <= m_blink_tm) {
			CacheRenderColor(renderer);
			SDL_SetRenderDrawColor(renderer, m_color.r, m_color.g, m_color.b, m_color.a);
			//RenderFillRect(renderer, &m_rect);
			SDL_RenderFillRectF(renderer, &m_rect);
			RestoreCachedRenderColor(renderer);
		}
		
		if (diff_ >= m_blink_tm * 2)
			m_start = SDL_GetTicks();
	}

private:
	SDL_FRect m_rect;
	uint32_t m_blink_tm;
	SDL_Color m_color;
};

struct EditBoxAttributes
{
	SDL_FRect rect = {0.f, 0.f, 80.f, 30.f};
	SDL_FRect placeholderRect = {10.f, 5.f, 80.f, 90.f};
	TextAttributes textAttributes = {"", {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff}};
	TextAttributes placeholderTextAttributes = {"", {0x00, 0x00, 0x00, 0xff}, {0xff, 0xff, 0xff, 0xff}};
	SDL_FRect margin = {0.f, 0.f, 100.f, 100.f};
	Font mem_font = Font::RobotoBold;
	std::string fontFile;
	Font placeholder_mem_font = Font::RobotoBold;
	std::string placeholderFontFile;
	FontStyle fontStyle = FontStyle::Normal;
	FontStyle defaultTxtFontStyle = FontStyle::Normal;
	float line_height = 0.f;
	EdgeType edgeType;
	uint32_t maxlines = 1;
	// max text size in code points
	uint32_t maxTextSize = 1024000;
	TextWrapStyle textWrapStyle = TextWrapStyle::MAX_CHARS_PER_LN;
	Gravity gravity = Gravity::Left;
	float cornerRadius = 0.f;
	int customFontstyle = 0x00;
	float lineSpacing = 0.f;
	float outline = 0.f;
	bool highlightOnHover = false;
	SDL_Color outlineColor = {0x00, 0x00, 0x00, 0x00};
	SDL_Color cursorColor = {0x00, 0x00, 0x00, 0xff};
	SDL_Color onHoverOutlineColor = {0x00, 0x00, 0x00, 0x00};
	SDL_Color onHoverBgColor = {0x00, 0x00, 0x00, 0x00};
	SDL_Color onHoverTxtColor = {0x00, 0x00, 0x00, 0x00};
	uint32_t cursorSpeed = 500;
};


class EditBox : public Context, public IView
{
public:
	EditBox() = default;
	int32_t id = (-1);

	EditBox& registerOnTextInputCallback(std::function<void(EditBox&)> _onTextInputCallback)
	{
		onTextInputCallback = _onTextInputCallback;
		return *this;
	}

	EditBox& onTextInputFilter(std::function<void(EditBox&, std::string&)> _onTextInputFilterCallback)
	{
		onTextInputFilterCallback = _onTextInputFilterCallback;
		return *this;
	}

	EditBox& Build(Context* _context, EditBoxAttributes& _attr)
	{
		Context::setContext(_context);
		adaptiveVsyncHD.setAdaptiveVsync(adaptiveVsync);
		IView::type = "editbox";
		bounds = _attr.rect;
		textRect = {
			bounds.x + to_cust(_attr.margin.x, bounds.w),
			bounds.y + to_cust(_attr.margin.y, bounds.h),
			to_cust(_attr.margin.w, bounds.w),
			to_cust(_attr.margin.h, bounds.h),
		};
		finalTextRect = textRect;
		line_height = std::clamp(_attr.line_height,0.f,255.f);
		if (line_height == 0.f) {
			line_height = textRect.h;
		}

		cornerRadius = _attr.cornerRadius;
		textAttributes = _attr.textAttributes;
		fontAttributes = { _attr.fontFile, _attr.fontStyle, static_cast<uint8_t>(line_height) };
		maxCodePointsPerLn = static_cast<uint32_t>(textRect.w / (textRect.h / 2.f));
		place_holder_text = _attr.placeholderTextAttributes.text;

		cursor.setContext(getContext());
		cursor.setColor(_attr.cursorColor);
		cursor.setBlinkTm(_attr.cursorSpeed);
		cursor.setRect({ textRect.x, textRect.y, to_cust(10.f, textRect.h), textRect.h });

		TTF_Font* tmpFont = nullptr;

		if (fontAttributes.font_file.empty())
		{
			Fonts[_attr.mem_font]->font_size = fontAttributes.font_size;
			fontAttributes.font_file = Fonts[_attr.mem_font]->font_name;
			tmpFont = FontSystem::Get().getFont(*Fonts[_attr.mem_font]);
		}

		charStore.setProps(getContext(), fontAttributes, _attr.customFontstyle);

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
				_attr.placeholderFontFile.c_str(), _attr.defaultTxtFontStyle, static_cast<uint8_t>(dflTxtRect.h) };

			if (plhFontAttr.font_file.empty())
			{
				Fonts[_attr.placeholder_mem_font]->font_size = plhFontAttr.font_size;
				plhFontAttr.font_file = Fonts[_attr.placeholder_mem_font]->font_name;
				tmpFont = FontSystem::Get().getFont(*Fonts[_attr.placeholder_mem_font]);
				//GLogger.Log(Logger::Level::Info, "place holder:", _attr.placeholderTextAttributes.text.c_str(), plhFontAttr.font_file);
			}

			FontSystem::Get().setFontAttributes({ plhFontAttr.font_file.c_str(), plhFontAttr.font_style, plhFontAttr.font_size }, 0);
			auto textTex = FontSystem::Get().genTextTextureUnique(renderer, _attr.placeholderTextAttributes.text.c_str(), _attr.placeholderTextAttributes.text_color);

			int test_w, test_h;
			SDL_QueryTexture(textTex.value().get(), nullptr, nullptr, &test_w, &test_h);
			if ((int)(test_w) < dflTxtRect.w)
			{
				dflTxtRect.w = (float)(test_w);
			}
			if (static_cast<float>(test_h) >= dflTxtRect.h)
			{
				dflTxtRect.y += static_cast<float>(TTF_FontDescent(tmpFont)); /*FontSystem::Get().getFont(_attr.placeholderFontFile, static_cast<int>(dflTxtRect.h))));*/
				dflTxtRect.h = static_cast<float>(test_h);
			}
			//GLogger.Log(Logger::Level::Info, "real pos:", pv->getRealX(), pv->getRealY());
			//GLogger.Log(Logger::Level::Info, "bounds rect{", bounds.x, bounds.y, bounds.w, bounds.h, "}");
			//GLogger.Log(Logger::Level::Info, "dflt txt rect{", dflTxtRect.x, dflTxtRect.y, dflTxtRect.w, dflTxtRect.h, "}");

			SDL_FRect dflSrc = { 0.f, 0.f, dflTxtRect.w, (float)test_h };
			SDL_FRect dflDst = { 0.f, 0.f, dflTxtRect.w, dflTxtRect.h };

			dflTxtTexture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
				SDL_TEXTUREACCESS_TARGET, static_cast<int>(dflTxtRect.w), static_cast<int>(dflTxtRect.h));
			CacheRenderTarget crt_(renderer);
			SDL_SetRenderTarget(renderer, dflTxtTexture.get());
			SDL_SetTextureBlendMode(dflTxtTexture.get(), SDL_BLENDMODE_BLEND);
			RenderClear(renderer, 0, 0, 0, 0);
			RenderTexture(renderer, textTex.value().get(), &dflSrc, &dflDst);
			crt_.release(renderer);
		}
		outlineColor = _attr.outlineColor;
		onHoverOutlineColor = _attr.onHoverOutlineColor;
		highlightOnHover = _attr.highlightOnHover;
		onHoverBgColor = _attr.onHoverBgColor;
		outlineRect.Build(this, bounds, _attr.outline, cornerRadius, textAttributes.bg_color, _attr.outlineColor);

		return *this;
	}

	void draw() override
	{
		outlineRect.draw();
		// fillRoundedRectF(renderer, rect, cornerRadius, textAttributes.bg_color);
		if (not hasFocus)
		{
			if (internalText.empty())
			{
				RenderTexture(renderer, dflTxtTexture.get(), nullptr, &dflTxtRect);
			}
			else
			{
				RenderTexture(renderer, txtTexture.get(), nullptr, &finalTextRect);
			}
		}
		else
		{
			if (not internalText.empty())
			{
				RenderTexture(renderer, txtTexture.get(), nullptr, &finalTextRect);
			}
			else
			{
				RenderTexture(renderer, dflTxtTexture.get(), nullptr, &dflTxtRect);
			}
			cursor.Draw();
		}

		if (onFocusView != nullptr and hasFocus)
			onFocusView->draw();

		// return *this;
	}

	SDL_FRect& getRect()
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

	EditBox& setOnFocusView(IView* _onFocusView)
	{
		onFocusView = _onFocusView;
		return *this;
	}

	IView* getOnFocusView()
	{
		return onFocusView;
	}

	bool isActive() const
	{
		return hasFocus;
	}

	EditBox& killFocus()
	{
		if (SDL_IsTextInputActive())
			SDL_StopTextInput();
		adaptiveVsyncHD.stopRedrawSession();
		hasFocus = false;
		if (highlightOnHover)
		{
			outlineRect.outline_color = outlineColor;
			outlineRect.color = textAttributes.bg_color;
		}
		if (onFocusView != nullptr)
			onFocusView->hide();
		//GLogger.Log(Logger::Level::Info, "kill focus");
		return *this;
	}

	bool handleEvent() override
	{
		bool result_ = false;

		if (onFocusView and hasFocus)
			if (onFocusView->handleEvent())
				return true;
		switch (event->type)
		{
		case EVT_WPSC:
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

			auto tmpRct = cursor.getRect();
			tmpRct =
			{
				DisplayInfo::Get().toUpdatedWidth(tmpRct.x),
				DisplayInfo::Get().toUpdatedHeight(tmpRct.y),
				DisplayInfo::Get().toUpdatedWidth(tmpRct.w),
				DisplayInfo::Get().toUpdatedHeight(tmpRct.h),
			};
			cursor.setRect(tmpRct);
			// fontAttributes.font_size = DisplayInfo::Get().toUpdatedHeight(fontAttributes.font_size);
			result_ = true;
		}
		break;
		case EVT_FINGER_DOWN:
		{
			if (onClick(event->tfinger.x * DisplayInfo::Get().RenderW, event->tfinger.y * DisplayInfo::Get().RenderH) and not hasFocus)
			{
				hasFocus = true;
				adaptiveVsyncHD.startRedrawSession();
				SDL_StartTextInput();
				////TODO: cache current vsync then SDL_RenderSetVSync(20)
				// SDL_GetRenderVSync(renderer, &prevVsync);
				// SDL_SetRenderVSync(renderer, 20);
				if (onFocusView)
					onFocusView->show();
				result_ = true;
			}
			else
			{
				if (hasFocus)
				{
					killFocus();
					/*
					//SDL_SetRenderVSync(renderer, prevVsync);
					SDL_StopTextInput(), hasFocus = false, adaptiveVsyncHD.stopRedrawSession();
					if (highlightOnHover)
					{
						outlineRect.outline_color = outlineColor;
						outlineRect.color = textAttributes.bg_color;
					}
					if (onFocusView)
						onFocusView->hide();
					*/
					//result_ = true;
				}
			}
		}
		break;
		/*case EVT_FINGER_MOTION:
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
		case EVT_MOUSE_MOTION:
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
		case EVT_FINGER_UP:
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
		case SDL_KEYDOWN:
		{
			if (hasFocus)
			{
				// SDL_Log("key down");
				cursor.m_start = SDL_GetTicks();
				if (event->key.keysym.scancode == SDL_SCANCODE_BACKSPACE and not internalText.empty())
				{
					internalText.erase(internalText.size() - (inputSize.back())),
						inputSize.pop_back();
					updateTextTexture("");
					if (onTextInputCallback)
						onTextInputCallback(*this);
				}
				if (event->key.keysym.scancode == SDL_SCANCODE_SELECT)
				{
				}
				if (event->key.keysym.scancode == SDL_SCANCODE_AC_BACK)
				{
					if (hasFocus)
					{
						killFocus();
						return true;
					}
				}
			}
		}
		break;
		case SDL_TEXTINPUT:
		{
			if (hasFocus)
			{
				// SDL_Log("txt input");
				cursor.m_start = SDL_GetTicks();
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
				return true;
			}
		}
		break;
		default:
			break;
		}

		return result_;
	}

	EditBox& setOnHoverOutlineColor(const SDL_Color& _color)
	{
		outlineRect.outline_color = _color;
		onHoverOutlineColor = _color;
		return *this;
	}

	EditBox& setOutlineColor(const SDL_Color& _color)
	{
		outlineRect.outline_color = _color;
		outlineColor = _color;
		return *this;
	}

	EditBox& clearAndSetText(const std::string& newText)
	{
		inputSize.clear();
		internalText.clear();
		for (const auto& ch_ : newText)
		{
			if (inputSize.size() + 1 <= maxTextSize)
			{
				inputSize.emplace_back(1);
			}
			else
			{
				//updateTextTexture(newText.substr(0, inputSize.size()));
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
			if (x < pv->getRealX() + bounds.x || x >(pv->getRealX() + bounds.x + bounds.w) || y < pv->getRealY() + bounds.y ||
				y >(pv->getRealY() + bounds.y + bounds.h))
				return false;
		}
		else if (axis == 1 /*x-axis only*/)
		{
			if (x < bounds.x or x >(bounds.x + bounds.w))
				return false;
		}
		else if (axis == 2 /*y-axis only*/)
		{
			if (y < bounds.y or y >(bounds.y + bounds.h))
				return false;
		}
		return true;
	}

protected:
	void updateTextTexture(const std::string& _newText)
	{
		internalText += _newText;

		if (internalText.empty())
		{
			cursor.setPosX(pv->getRealX() + textRect.x);
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
		if (txtTexture) {
			txtTexture.reset();
		}
		txtTexture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
			SDL_TEXTUREACCESS_TARGET, textRect.w, textRect.h);
		SDL_SetTextureBlendMode(txtTexture.get(), SDL_BLENDMODE_BLEND);
		SDL_SetRenderTarget(renderer, txtTexture.get());

		RenderClear(renderer, 0, 0, 0, 0);
		float tab = 0.f,cw=0.f,lw=0.f;
		/*std::for_each(internalText.rbegin(), internalText.rend(), [this,&cw,&lw,&tab](char* _char) {
			std::string outStr = { _char };
			auto chtx = charStore.getChar(outStr, textAttributes.text_color);
			int tmpW, tmpH;
			SDL_QueryTexture(chtx, nullptr, nullptr, &tmpW, &tmpH);
			lw = std::clamp((float)tmpW, 1.f, line_height / 1.5f);
			SDL_FRect dst = { 1.f + cw,0.f,lw,line_height };
			if (chtx != nullptr) {
				RenderTexture(renderer, chtx, nullptr, &dst);
			}
			tab += 1.f;
			cw += 1.f + lw;
			});*/

		for (auto& _char : internalText) {
			std::string outStr = { _char };
			auto chtx=charStore.getChar(outStr, textAttributes.text_color);
			int tmpW, tmpH;
			SDL_QueryTexture(chtx, nullptr, nullptr, &tmpW, &tmpH);
			lw = std::clamp((float)tmpW, 1.f, line_height / 1.5f);
			SDL_FRect dst = { 1.f+cw,0.f,lw,line_height };
			// check for overflow on the right side
			/*if (dst.x + dst.w > textRect.x + textRect.w) {
				lw = 0.f;
				break;
			}*/
			if (getRealX() + textRect.x + cw - lw > textRect.x + textRect.w) {
				lw = 0.f;
				break;
			}
			if (chtx != nullptr) {
				RenderTexture(renderer, chtx, nullptr, &dst);
			}
			tab += 1.f;
			cw += 1.f+lw;
		}
		rTargetCache.release(renderer);
		cursor.setPosX(getRealX() + textRect.x + cw-lw);
		/*GLogger.Log(Logger::Level::Info, "cursor x:" + std::to_string(cursor.getRect().x) + ", y:" + std::to_string(cursor.getRect().y) + ", w:" +
			std::to_string(cursor.getRect().w) + ", h:" + std::to_string(cursor.getRect().h));*/
		finalTextRect.w = textRect.w;
	}

protected:
	IView* onFocusView = nullptr;

protected:
	CharStore charStore{};
	float cornerRadius = 0.f,line_height=0.f;
	std::shared_ptr<SDL_Texture> txtTexture;
	std::shared_ptr<SDL_Texture> dflTxtTexture;
	std::function<void(EditBox&)> onTextInputCallback;
	std::function<void(EditBox&)> onKeyPressEnter;
	std::function<void(EditBox&, std::string&)> onTextInputFilterCallback;
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
	Cursor cursor;
	uint32_t maxCodePointsPerLn = 1;
	AdaptiveVsyncHandler adaptiveVsyncHD, updAdaptiveVsyncHD;
	bool highlightOnHover = false;
	SDL_Color onHoverOutlineColor = { 0x00, 0x00, 0x00, 0x00 };
	SDL_Color onHoverBgColor = { 0x00, 0x00, 0x00, 0x00 };
	SDL_Color onHoverTxtColor = { 0x00, 0x00, 0x00, 0x00 };
	SDL_Color outlineColor = { 0x00, 0x00, 0x00, 0x00 };
	int prevVsync = 1200;
};


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

	void draw(SDL_Renderer *renderer) const
	{
		RenderTexture(renderer, this->texture.get(), nullptr, &dest);
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
		RenderClear(renderer, 0, 0, 0, 0);

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






class ToggleButton;

enum class BtnState {
	ON,
	OFF,
};

struct ToggleButtonAttr {
	SDL_FRect rect{ 0.f,0.f,0.f,0.f };
	// background color when OFF
	SDL_Color bg{ 0xff,0xff,0xff,0xff };
	// background color when ON
	SDL_Color bg_on_color{ 0x00,0xff,0x00,255 };
	// dot background color when OFF
	SDL_Color dot_color{ 0x00,0x00,0x00,0xff };
	// dot background color when ON
	SDL_Color dot_on_color{ 0x00,0x00,0x00,0xff };
	SDL_Color outline_color{ 0x00,0xff,0x00,255 };
	float outline_px = 0.f;
	// inner dot radius in percentage
	float dot_rad_px = 85.f;
	// button's initial state defalut is OFF
	BtnState default_state = BtnState::OFF;
	// callback invoked when state changes. default is nullptr
	std::function<void(ToggleButton&)> onToggle = nullptr;
};

class ToggleButton: public Context, public IView {
public:
	ToggleButton(){}

	ToggleButton(Context* _context, ToggleButtonAttr tbr) {
		Build(_context, tbr);
	}

	ToggleButton& Build(Context* _context, ToggleButtonAttr tbr) {
		setContext(_context);
		attr = tbr;
		state = attr.default_state;
		//GLogger.Log(Logger::Level::Debug, "org dot rect{", attr.rect.x, attr.rect.y, attr.rect.w, attr.rect.h, "}");
		if (attr.outline_px > 0.f) {
			outline_px=to_cust(attr.outline_px, attr.rect.h);
			attr.rect = {
				attr.rect.x + to_cust(attr.outline_px,attr.rect.h),
				attr.rect.y + to_cust(attr.outline_px,attr.rect.h),
				attr.rect.w - (to_cust(attr.outline_px,attr.rect.h) * 2.f),
				attr.rect.h - (to_cust(attr.outline_px,attr.rect.h) * 2.f)
			};
		}
		bounds = attr.rect;
		//if (attr.rect.w > attr.rect.h) {
		dotr = ph(attr.dot_rad_px / 2.f);
		dotx = state == BtnState::OFF ? (dotr + bounds.x + (ph(100.f - attr.dot_rad_px) / 2.f)) : ((dotr + bounds.x + bounds.w) - ph(attr.dot_rad_px) - (ph(100.f - attr.dot_rad_px)));
		doty = dotr + bounds.y + ph(100.f - attr.dot_rad_px) / 2.f;
		//}
		/*else if (attr.rect.w < attr.rect.h or attr.rect.w == attr.rect.h) {
			dotr = pw(attr.dot_rad_px/2.f);
			dotx = bounds.x + pw(100.f - attr.dot_rad_px) / 2.f;
			doty = bounds.y + pw(100.f - attr.dot_rad_px) / 2.f;
		}*/
		/*GLogger.Log(Logger::Level::Debug, "dot rect{", bounds.x, bounds.y, bounds.w, bounds.h, "}");
		GLogger.Log(Logger::Level::Debug, "dot parm{ dotr:",dotr,"dotx:",dotx,"doty:",doty,"}");*/
		return *this;
	}

	bool handleEvent()override final
	{
		bool result = false;
		switch (event->type)
		{
		case EVT_MOUSE_BTN_DOWN:
			if (pointInBound(event->motion.x, event->motion.y))
			{
				//GLogger.Log(Logger::Level::Debug, "point in bound");
				key_down = true, result = true;
			}
			break;
		case EVT_MOUSE_MOTION:
			key_down = false;
			break;
		case EVT_MOUSE_BTN_UP:
		{
			if (key_down) {
				toggleState();
				if (nullptr != attr.onToggle) {
					attr.onToggle(*this);
				}
				result = true;
			}
			key_down = false;
		}
			break;
		default:
			break;
		}
		return result;
	}

	void onUpdate()override final
	{

	}

	void draw()override final
	{
		/*if (attr.outline_px > 0.f) {
			fillRoundedRectOutline(renderer, bounds, 100.f, outline_px, attr.outline_color);
		}*/

		fillRoundedRectF(renderer, bounds, 100.f, state==BtnState::OFF?attr.bg:attr.bg_on_color);
		draw_filled_circle(renderer, dotx, doty,dotr , state == BtnState::OFF ? attr.dot_color : attr.dot_on_color);
	}

	BtnState getState() const {
		return state;
	}

	void setState(BtnState _state) {
		state = _state == BtnState::ON ? BtnState::ON : BtnState::OFF;
		dotx = state == BtnState::OFF ? (dotr + bounds.x + (ph(100.f - attr.dot_rad_px) / 2.f)) : ((dotr + bounds.x + bounds.w) - ph(attr.dot_rad_px) - (ph(100.f - attr.dot_rad_px)));
	}
private:
	bool pointInBound(const float& x, const float& y) const noexcept
	{
		if ((x >= pv->getRealX() + bounds.x) && (x < pv->getRealX() + bounds.x + bounds.w) 
			&& (y >= pv->getRealY() + bounds.y) && (y < pv->getRealY() + bounds.y + bounds.h))
			return true;
		return false;
	}

	void toggleState() {
		state = state == BtnState::ON ? BtnState::OFF : BtnState::ON;
		dotx = state == BtnState::OFF ? (dotr + bounds.x + (ph(100.f - attr.dot_rad_px) / 2.f)) : ((dotr + bounds.x + bounds.w) - ph(attr.dot_rad_px) - (ph(100.f - attr.dot_rad_px)));
		if (attr.onToggle != nullptr) {
			attr.onToggle(*this);
		}
	}
private:
	ToggleButtonAttr attr;
	float dotx = 0.f, doty = 0.f, dotr = 80.f, outline_px=0.f;
	bool key_down = false;
	BtnState state{};
};


/*
* Breathe. I inspected the active file. You already started integrating ScrollView earlier; to make kinetic scrolling and the scrollbar thumb drive the same state we need two-way synchronization:
•	when existing kinetic scroll code moves content, update the ScrollView offset.
•	when ScrollView moves (thumb/track/wheel), update the content position / internal scroll state.
•	unify clamping and units (pixels). Always use the same contentLength and viewportLength for both.
Below are minimal, safe changes you can paste into ..\..\Desktop\GM's\volt m win\voltm.hpp. They:
1.	add members to Cell to hold the ScrollView and sync state,
2.	provide helper sync methods (syncScrollToView() and syncScrollFromView()),
3.	replace the event/scroll handling to keep both in sync (wheel, drag, kinetic fling).
Make the edits in the Cell implementation area (place with other members / methods for Cell). If your Cell class is named differently, adapt the identifier accordingly.
Note: I assume ScrollView provides:
•	bool handleEvent(const SDL_Event&, const SDL_FRect &parentBounds) — true when offset changed,
•	float getScrollOffset() const,
•	void setScrollOffset(float),
•	void setContentLength(float),
•	void setViewportLength(float),
•	void setBounds(const SDL_FRect&),
•	void draw(SDL_Renderer*) or draw(renderer).
If method names differ adjust accordingly.

// Add into the Cell class members (near other private members)
private:
	// integrated ScrollView instance
	ScrollView scrollView{};
	bool useScrollView = false;           // whether we created and use the scroll view
	float scrollViewPrevOffset = 0.f;     // last known ScrollView offset
	// The cell keeps its own "content scroll" state (existing kinetic code likely updates this)
	float contentScrollY = 0.f;           // pixels scrolled (0 .. max_scroll)
	float max_scroll = 0.f;               // existing maximum scroll value for content

	// Helper: clamp offset to valid range
	static inline float clampScroll(float v, float minV, float maxV) {
		return std::min(std::max(v, minV), maxV);
	}

	// Call when the content layout changes (e.g. updateMaxScroll). Ensures ScrollView sizes match.
	void ensureScrollViewSetup() {
		if (!useScrollView) return;
		float contentLength = std::max(bounds.h, bounds.h + max_scroll); // total content height
		scrollView.setContentLength(contentLength);
		scrollView.setViewportLength(bounds.h);
		// ensure scrollView offset and contentScrollY are in-range and consistent
		contentScrollY = clampScroll(contentScrollY, 0.f, std::max(0.f, contentLength - bounds.h));
		scrollView.setScrollOffset(contentScrollY);
		scrollViewPrevOffset = contentScrollY;
	}

	// Sync content -> ScrollView (call from your kinetic scroll code)
	void syncScrollToView() {
		if (!useScrollView) return;
		// Keep ScrollView thumb in sync with contentScrollY
		float contentLength = std::max(bounds.h, bounds.h + max_scroll);
		scrollView.setContentLength(contentLength);
		scrollView.setViewportLength(bounds.h);
		float clamped = clampScroll(contentScrollY, 0.f, std::max(0.f, contentLength - bounds.h));
		scrollView.setScrollOffset(clamped);
		scrollViewPrevOffset = clamped;
	}

	// Sync ScrollView -> content (call after ScrollView reports changes)
	void syncScrollFromView() {
		if (!useScrollView) return;
		float newOffset = scrollView.getScrollOffset();
		newOffset = clampScroll(newOffset, 0.f, std::max(0.f, std::max(bounds.h, bounds.h + max_scroll) - bounds.h));
		float delta = newOffset - scrollViewPrevOffset;
		if (delta != 0.f) {
			// when ScrollView moves down (offset increases), we need to move children up => negative
			updatePosByInternal(0.f, -delta); // reuse your existing function that shifts children
			contentScrollY = newOffset;
			scrollViewPrevOffset = newOffset;
			redraw = true; // ensure redraw (use your existing redraw flag)
		}
	}


	Replace/augment event handling in the Cell (where you previously forwarded events to the scroll or handled finger/mouse drag/wheel) with the snippet below. This ensures:
•	events handled by the ScrollView update content via syncScrollFromView(),
•	wheel/kinetic updates to contentScrollY call syncScrollToView().



// In Cell::handleEvent(...) where you process events for scrolling:
if (useScrollView) {
	// First, let ScrollView handle pointer / thumb / wheel events.
	// If it handled and changed offset, reflect that into content.
	if (scrollView.handleEvent(*event, bounds)) {
		syncScrollFromView();
		return true; // event consumed by scrollbar
	}
}

// --- Existing kinetic / manual scroll code branch ---
// Wherever your code updates contentScrollY (dragging, fling, wheel handling outside ScrollView),
// after you mutate contentScrollY, call:
{
	// Example after contentScrollY += dy;
	// clamp contentScrollY to [0, max]
	float contentLength = std::max(bounds.h, bounds.h + max_scroll);
	contentScrollY = clampScroll(contentScrollY, 0.f, std::max(0.f, contentLength - bounds.h));
	// move children visually (existing behavior)
	updatePosByInternal(0.f, - (contentScrollY - previousContentScrollY));
	// now update the ScrollView to match the manual move
	syncScrollToView();
	previousContentScrollY = contentScrollY; // keep your existing state
}


Also ensure you call ensureScrollViewSetup() whenever bounds.h or max_scroll changes (for example inside updateMaxScroll(...) or layout code). Example:
// where you compute max_scroll or adjust bounds.h
max_scroll = std::max(max_scroll, computed_content_height - bounds.h);
if (!useScrollView && computed_content_height > bounds.h) {
	// initialize ScrollView bounds (right edge) once
	SDL_FRect svBounds = {
		bounds.x + bounds.w - to_cust(4.f, bounds.w),
		bounds.y,
		to_cust(4.f, bounds.w),
		bounds.h
	};
	scrollView.setBounds(svBounds);
	useScrollView = true;
	// set sizes & offset
	float contentLength = std::max(bounds.h, computed_content_height);
	scrollView.setContentLength(contentLength);
	scrollView.setViewportLength(bounds.h);
	scrollView.setScrollOffset(contentScrollY);
	scrollViewPrevOffset = contentScrollY;
	has_scroll_bar = true; // keep your existing flag if needed
} else if (useScrollView) {
	// keep sizes in sync
	ensureScrollViewSetup();
}

Finally, in your draw path draw the ScrollView after drawing children so the thumb appears above content:

// In Cell::draw() after drawing content:
if (useScrollView) {
	scrollView.draw(renderer);
}



Why this solves the problem
•	Two-way sync: any code that moves content updates the scrollView via syncScrollToView(). Any user interaction with the scrollView updates the content via syncScrollFromView().
•	Clamping and contentLength/viewportLength are set in one place to avoid drift.
•	Wheel, drag, fling—wherever you update contentScrollY—must call syncScrollToView() afterwards.
Checklist for you
•	Confirm the ScrollView API names match — adjust getScrollOffset / setScrollOffset / handleEvent names if different.
•	Replace updatePosByInternal usage if your function is named differently (I used the name from earlier messages).
•	Make sure previousContentScrollY (or equivalent) exists in your kinetic code to compute deltas.
•	Call ensureScrollViewSetup() on resize and after content layout passes.
If you want I can:
•	produce an exact in-file patch replacing specific functions (I can open locations if you point to the Cell class start or the functions that currently implement kinetic scrolling: names/line numbers),
•	or search for your kinetic scroll symbols (contentScrollY, scroll_y, updatePosByInternal, previousContentScrollY) and prepare a single automated edit.
Which do you prefer — a full patch applied to voltm.hpp or help me locate the exact kinetic-scroll functions to patch automatically?

*/


/*
 ---------- ScrollView ----------
 Usage notes:
  - Create a ScrollView, call setBounds(), setContentSize() inside the view that needs scrolling, then use
  - Forward pointer/mouse/touch events to ScrollView::handleEvent(...).
  - handleEvent(&event, parent_bounds) and draw(renderer).
  - Query getScrollOffset() to translate content when drawing content.
  - When the scroll offset changes, translate children drawing by the offset (apply delta via updatePosByInternal or temporarily shift child positions while drawing).
Notes and tips
•	The ScrollView stores a pixel offset (0..contentLength - viewport). We applied that offset by calling updatePosByInternal(0, -delta) to move child views in the opposite direction of the scrollbar thumb motion. That keeps the child bounds in sync with the visual position.
•	If you prefer non-mutating rendering (safer): instead of mutating child bounds, temporarily translate before draw and restore after:
•	Save children positions (or maintain a function to draw children at an offset).
•	Draw at y - scrollView.getScrollOffset() and then restore state.
•	Event forwarding:
•	When forwarding events to ScrollView::handleEvent(...) pass the same parent bounds you used to set the ScrollView bounds.
•	The scrollView expects screen coordinates, so make sure you forward raw event coordinates (no extra translate).
•	Accessibility with touch/wheel:
•	ScrollView::handleEvent supports mouse wheel, drag, clicking the track; it will automatically synthesize thumb movement. Keep the existing finger/drag logic if you still want kinetic scrolling; otherwise you can centralize all scrolling through ScrollView.
•	To find the methods quickly in Visual Studio: use Edit > Find and Replace or the Solution Explorer to jump to Cell and updateMaxScroll. (Or open the active file: the active document is voltm.hpp.)
If you want, I can:
•	Convert existing kinetic scroll behavior to drive the new ScrollView (so thumb and fling integrate automatically).
Which would you prefer — a full in-file patch, or the small snippets above for you to paste?
 */
class ScrollView
{
public:
	// Visual configuration
	struct Style
	{
		SDL_Color trackColor = { 0x20, 0x20, 0x20, 0xFF };
		SDL_Color thumbColor = { 0xA0, 0xA0, 0xA0, 0xFF };
		float trackThickness = 8.0f;   // px (for vertical scroll this is width)
		float thumbMinLength = 20.0f;  // px
		bool vertical = true;          // vertical (true) or horizontal (false)
	};

	ScrollView() = default;

	ScrollView(const SDL_FRect& bounds, float contentLen, const Style& style = Style())
		: bounds_(bounds), style_(style)
	{
		setContentLength(contentLen);
	}

	ScrollView& Build(const SDL_FRect& bounds, float contentLen, const Style& style = Style()) {
		bounds_ = bounds;
		style_ = style;
		setContentLength(contentLen);
		return *this;
	}

	// Set / update the widget bounds (in parent coordinate space)
	void setBounds(const SDL_FRect& b)
	{
		bounds_ = b;
		computeThumbRect();
	}

	// Set total content length (height for vertical, width for horizontal)
	void setContentLength(float contentLength)
	{
		contentLength_ = std::max(0.0f, contentLength);
		computeThumbRect();
	}

	// Set viewport length (visible area length). Useful if viewport changes.
	void setViewportLength(float viewportLength)
	{
		viewportLength_ = std::max(0.0f, viewportLength);
		computeThumbRect();
	}

	// Set scroll offset (0..max). Clamp internally.
	void setScrollOffset(float offset)
	{
		float maxOff = maxScrollOffset();
		if (maxOff <= 0.0f)
		{
			scrollOffset_ = 0.0f;
		}
		else
		{
			scrollOffset_ = std::clamp(offset, 0.0f, maxOff);
		}
		computeThumbRect();
	}

	// Increment scroll offset by delta (positive scrolls down/right)
	inline void scrollBy(float delta)
	{
		setScrollOffset(scrollOffset_ + delta);
	}

	// Scroll by a page (positive = forward)
	inline void pageScroll(int pages = 1)
	{
		setScrollOffset(scrollOffset_ + pages * viewportLength_);
	}

	// Get current scroll offset
	inline float getScrollOffset() const { return scrollOffset_; }

	// Get scroll fraction (0..1)
	inline float getScrollFraction() const
	{
		float maxOff = maxScrollOffset();
		return maxOff <= 0.0f ? 0.0f : (scrollOffset_ / maxOff);
	}

	// Handle SDL events. Returns true if event was consumed.
	// parentBounds is used to convert mouse coords if necessary (pass same as setBounds if already in global coords).
	bool handleEvent(const SDL_Event& ev, const SDL_FRect& parentBounds)
	{
		// Convert mouse coords for float rect check
		auto isPointInBounds = [&p=parentBounds](float px, float py, const SDL_FRect& r) -> bool {
			return ((px >= p.x+r.x) && (px < (p.x+r.x + r.w)) && (py >= p.y+r.y) && (py < (p.y+r.y + r.h)));
			};

		auto isPointInRect = [](float px, float py, const SDL_FRect& r) -> bool {
			return ((px >= r.x) && (px < (r.x + r.w)) && (py >= r.y) && (py < (r.y + r.h)));
			};

		switch (ev.type)
		{
		case SDL_MOUSEWHEEL:
			// Mouse wheel: vertical scroll uses y, horizontal uses x
			if (isPointInRect(ev.wheel.x * wheelStep(), ev.wheel.y * wheelStep(), parentBounds)) {
				if (style_.vertical)
					scrollBy(-ev.wheel.y * wheelStep());
				else
					scrollBy(-ev.wheel.x * wheelStep());
				return true;
			}
			else {
				return false;
			}
		case SDL_MOUSEBUTTONDOWN:
		{
			float mx = static_cast<float>(ev.button.x);
			float my = static_cast<float>(ev.button.y);
			if (!isPointInBounds(mx, my, bounds_)) return false;

			// If click is on thumb -> start dragging
			if (isPointInBounds(mx, my, thumbRect_))
			{
				dragging_ = true;
				dragStartPos_ = style_.vertical ? my : mx;
				dragStartOffset_ = scrollOffset_;
				return true;
			}

			// If click on track above/before thumb -> page up
			if (style_.vertical)
			{
				if (my < thumbRect_.y)
					pageScroll(-1);
				else if (my > (thumbRect_.y + thumbRect_.h))
					pageScroll(1);
			}
			else
			{
				if (mx < thumbRect_.x)
					pageScroll(-1);
				else if (mx > (thumbRect_.x + thumbRect_.w))
					pageScroll(1);
			}
			return true;
		}
		case SDL_MOUSEBUTTONUP:
		{
			if (dragging_)
			{
				dragging_ = false;
				return true;
			}
			break;
		}
		case SDL_MOUSEMOTION:
		{
			if (!dragging_) return false;
			float pos = style_.vertical ? static_cast<float>(ev.motion.y) : static_cast<float>(ev.motion.x);
			float delta = pos - dragStartPos_;
			// Translate delta in pixels on track to content offset change
			float trackLen = trackLength();
			if (trackLen <= 0.0f || thumbLengthPx() <= 0.0f) return true;
			float movablePx = std::max(0.0f, trackLen - thumbLengthPx());
			if (movablePx <= 0.0f) return true;
			float ratio = delta / movablePx;
			float contentDelta = ratio * std::max(0.0f, contentLength_ - viewportLength_);
			setScrollOffset(dragStartOffset_ + contentDelta);
			return true;
		}
		default:
			break;
		}

		return false;
	}

	// Draw the track and thumb into renderer. Caller should set blend/color states as desired.
	inline void draw(SDL_Renderer* renderer) const
	{
		// Track rect (float) and thumbRect_ are already float rectangles.
		SDL_FRect track = trackRect();
		// Draw track
		SDL_SetRenderDrawColor(renderer, style_.trackColor.r, style_.trackColor.g, style_.trackColor.b, style_.trackColor.a);
		SDL_RenderFillRectF(renderer, &track);
		// Draw thumb
		SDL_SetRenderDrawColor(renderer, style_.thumbColor.r, style_.thumbColor.g, style_.thumbColor.b, style_.thumbColor.a);
		SDL_RenderFillRectF(renderer, &thumbRect_);
	}

	// Return internal bounds
	inline SDL_FRect bounds() const { return bounds_; }

	// Compute and return the thumb rectangle (for external use)
	inline SDL_FRect thumbRect() const { return thumbRect_; }

	// Orientation helpers
	inline void setVertical(bool v) { style_.vertical = v; computeThumbRect(); }
	inline bool isVertical() const { return style_.vertical; }

	// Expose style (copy) for tweaks
	inline void setStyle(const Style& s) { style_ = s; computeThumbRect(); }
	inline Style style() const { return style_; }

private:
	// compute sizes and thumb rect. called whenever bounds, content length or viewport changes — this ensures thumbRect_.w/h/x/y are reliably and always set.
	void computeThumbRect()
	{
		// Ensure viewport length is set; if not, infer from bounds
		if (style_.vertical)
		{
			if (viewportLength_ <= 0.0f) viewportLength_ = bounds_.h;
		}
		else
		{
			if (viewportLength_ <= 0.0f) viewportLength_ = bounds_.w;
		}

		// Track rect
		SDL_FRect tr = trackRect();

		// Thumb length (in pixels) proportional to visible fraction
		float visibleFrac = (contentLength_ <= 0.0f) ? 1.0f : (viewportLength_ / std::max(viewportLength_, contentLength_));
		float thumbLen = visibleFrac * trackLength();
		thumbLen = std::max(style_.thumbMinLength, thumbLen);

		// Ensure thumb fits
		thumbLen = std::min(thumbLen, trackLength());

		// Thumb position: map scrollOffset (0..max) to track range
		float maxOff = maxScrollOffset();
		float thumbPosOnTrack = 0.0f;
		if (maxOff <= 0.0f)
		{
			thumbPosOnTrack = 0.0f;
		}
		else
		{
			float movable = std::max(0.0f, trackLength() - thumbLen);
			thumbPosOnTrack = (scrollOffset_ / maxOff) * movable;
		}

		// Set thumb rect (complete fields)
		if (style_.vertical)
		{
			thumbRect_.x = tr.x;
			thumbRect_.y = tr.y + thumbPosOnTrack;
			thumbRect_.w = tr.w;
			thumbRect_.h = thumbLen;
		}
		else
		{
			thumbRect_.x = tr.x + thumbPosOnTrack;
			thumbRect_.y = tr.y;
			thumbRect_.w = thumbLen;
			thumbRect_.h = tr.h;
		}
	}

	// Track rectangle (area where thumb moves)
	inline SDL_FRect trackRect() const
	{
		if (style_.vertical)
		{
			// centered vertical track within bounds.x..bounds.x + trackThickness
			float cx = bounds_.x + (bounds_.w - style_.trackThickness);
			return SDL_FRect{ cx, bounds_.y, style_.trackThickness, bounds_.h };
		}
		else
		{
			float cy = bounds_.y + (bounds_.h - style_.trackThickness);
			return SDL_FRect{ bounds_.x, cy, bounds_.w, style_.trackThickness };
		}
	}

	// Length of track (pixels thumb can travel + thumb length)
	inline float trackLength() const
	{
		SDL_FRect tr = trackRect();
		return style_.vertical ? tr.h : tr.w;
	}

	// Thumb length in px (helper)
	inline float thumbLengthPx() const
	{
		return style_.vertical ? thumbRect_.h : thumbRect_.w;
	}

	// Maximum scroll offset (contentLen - viewportLen)
	inline float maxScrollOffset() const
	{
		return std::max(0.0f, contentLength_ - viewportLength_);
	}

	// Default wheel step equals 1/10 of viewport length
	inline float wheelStep() const
	{
		return std::max(1.0f, viewportLength_ * 0.1f);
	}

private:
	SDL_FRect bounds_{ 0, 0, 0, 0 };   // full widget bounds
	SDL_FRect thumbRect_{ 0, 0, 0, 0 }; // previously lvl_rect -- now always fully set

	float contentLength_ = 0.0f;  // total content length (height for vertical)
	float viewportLength_ = 0.0f; // visible length (height)
	float scrollOffset_ = 0.0f;   // current content offset (0..max)

	// Dragging state
	bool dragging_ = false;
	float dragStartPos_ = 0.0f;
	float dragStartOffset_ = 0.0f;

	Style style_;
};




struct ScrollAttributes
{
    SDL_FRect rect = {};
    float min_val = 0.f, max_val = 1.f, start_val = 0.f, corner_radius = 0.f;
    SDL_Color bg_color = {50, 50, 50, 255};
    SDL_Color level_bar_color = {255, 255, 255, 255};
    Orientation orientation = Orientation::HORIZONTAL;
};

class Scroll : public Context, public IView
{
public:
    Scroll &Build(Context *_context, ScrollAttributes _attr)
    {
        setContext(_context);
        rect = _attr.rect;
        lvl_rect = rect;
        bg_color = _attr.bg_color;
        lvl_bar_color = _attr.level_bar_color;
        orientation = _attr.orientation;
        corner_radius = _attr.corner_radius;
        updateMinMaxValues(_attr.min_val, _attr.max_val, _attr.start_val);
        return *this;
    }

    bool handleEvent() override final
    {
        bool result = false;
        switch (event->type)
        {
            case EVT_MOUSE_BTN_DOWN:
                if (pointInBound(event->motion.x, event->motion.y))
                {
                    key_down = true, result = true;
                    if (Orientation::VERTICAL == orientation)
                        updateValue(screenToWorld(event->motion.y - (pv->getRealY() + rect.y)));
                    else if (Orientation::HORIZONTAL == orientation)
                        updateValue(screenToWorld(event->motion.x - (pv->getRealX() + rect.x)));
                }
                break;
            case EVT_MOUSE_MOTION:
                if (key_down)
                {
                    result = true;
                    if (Orientation::VERTICAL == orientation)
                        updateValue(screenToWorld(event->motion.y - (pv->getRealY() + rect.y)));
                    else if (Orientation::HORIZONTAL == orientation)
                        updateValue(screenToWorld(event->motion.x - (pv->getRealX() + rect.x)));
                }
                break;
            case EVT_MOUSE_BTN_UP:
                key_down = false;
                break;
            default:
                break;
        }
        return result;
    }

    inline void draw() final
    {
        if (Orientation::HORIZONTAL == orientation)
        {
            SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
            fillRoundedRectF(renderer, rect, corner_radius, bg_color);
            SDL_SetRenderDrawColor(renderer, lvl_bar_color.r, lvl_bar_color.g, lvl_bar_color.b, lvl_bar_color.a);
            fillRoundedRectF(renderer, lvl_rect, corner_radius, lvl_bar_color);
        }
        else if (Orientation::VERTICAL == orientation)
        {
            SDL_SetRenderDrawColor(renderer, lvl_bar_color.r, lvl_bar_color.g, lvl_bar_color.b, lvl_bar_color.a);
            fillRoundedRectF(renderer, rect, corner_radius, lvl_bar_color);
            SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
            fillRoundedRectF(renderer, lvl_rect, corner_radius, bg_color);
        }
    }

    inline Scroll &registerOnValueUpdateCallback(std::function<void(Scroll &)> onValUpdateClbk)
    {
        onValUpdate = std::move(onValUpdateClbk);
        return *this;
    }

    inline auto getCurrentValue() const
    {
        if (Orientation::VERTICAL == orientation)
            return max_val - (current_val - min_pad);
        return current_val - min_pad;
    }

    inline auto getMinValue()const { return min_val - min_pad; }
    inline auto getMaxValue()const { return max_val - max_pad; }
    inline auto isKeyDown()const noexcept { return key_down; }
    inline float getLevelLength() const
    {
        if (Orientation::HORIZONTAL == orientation)
            return lvl_rect.w;
        else if (Orientation::VERTICAL == orientation)
            return rect.h - lvl_rect.h;
        else if (Orientation::ANGLED == orientation)
            return 0.f;
    }

    inline Scroll &updateValue(float _val)
    {
        current_val = std::clamp(_val + min_pad, min_val, max_val);
        const float cr = (current_val * ratio);
        if (Orientation::HORIZONTAL == orientation)
        {
			//lvl_rect.w = cr;
            lvl_rect.x = rect.x + cr - (lvl_rect.w / 2.f);
        }
        else if (Orientation::VERTICAL == orientation)
        {
			//lvl_rect.h = cr;
            lvl_rect.y = rect.y + cr - (lvl_rect.h / 2.f);
        }
        if (onValUpdate)
            onValUpdate(*this);
        return *this;
    }

    inline Scroll &updateMinMaxValues(float _min_val, float _max_val, float _current_val)
    {
        min_val = _min_val;
        max_val = _max_val;
        // possible integer overflow bug if min/max val > INT_MAX/2
        // should test & add a static assert
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

    inline void updatePosBy(float dx, float dy) override final
    {
        rect.x += dx;
        rect.y += dy;
        lvl_rect.x += dx;
        lvl_rect.y += dy;
    }

private:
    inline bool pointInBound(const float &x, const float &y) const noexcept
    {
        if (Orientation::VERTICAL == orientation)
        {
            if ((y >= pv->getRealY() + rect.y) && (y < pv->getRealY() + rect.y + rect.h))
                return true;
        }
        else if (Orientation::HORIZONTAL == orientation)
        {
            if ((x >= pv->getRealX() + rect.x) && (x < pv->getRealX() + rect.x + rect.w))
                return true;
        }
        else if (Orientation::ANGLED == orientation)
            return false;
        return false;
    }

    [[nodiscard]] inline float screenToWorld(const float val) const
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
    float corner_radius = 0.f;
    SDL_FRect rect, lvl_rect;
    bool key_down = false;
    float ratio = 0.f;
    float length = 0.f;
    std::function<void(Scroll &)> onValUpdate = nullptr;
};








struct SliderAttributes
{
	SDL_FRect rect = {};
	float min_val = 0.f, max_val = 1.f, start_val = 0.f,
		  knob_radius = 0.f, corner_radius = 0.f;
	bool show_knob = true;
	SDL_Color bg_color = {50, 50, 50, 255};
	SDL_Color level_bar_color = {255, 255, 255, 255};
	SDL_Color knob_color = {255, 255, 255, 255};
	Orientation orientation = Orientation::HORIZONTAL;
};

class Slider : public Context, public IView
{
public:
	Slider &Build(Context *_context, SliderAttributes _attr)
	{
		setContext(_context);
		knob.setContext(_context);
		knob.Build(0.f, 0.f,
				   to_cust(_attr.knob_radius,
						   _attr.orientation == Orientation::HORIZONTAL ? _attr.rect.h : _attr.rect.w),
				   _attr.knob_color);
		rect = _attr.rect;
		lvl_rect = rect;
		bg_color = _attr.bg_color;
		lvl_bar_color = _attr.level_bar_color;
		orientation = _attr.orientation;
		show_knob = _attr.show_knob;
		corner_radius = _attr.corner_radius;
		updateMinMaxValues(_attr.min_val, _attr.max_val, _attr.start_val);
		return *this;
	}

	bool handleEvent() override
	{
		bool result = false;
		switch (event->type)
		{
		case EVT_MOUSE_BTN_DOWN:
			if (pointInBound(event->motion.x, event->motion.y))
			{
				key_down = true, result = true;
				if (Orientation::VERTICAL == orientation)
					updateValue(screenToWorld(event->motion.y - (pv->getRealY() + rect.y)));
				else if (Orientation::HORIZONTAL == orientation)
					updateValue(screenToWorld(event->motion.x - (pv->getRealX() + rect.x)));
			}
			break;
		case EVT_MOUSE_MOTION:
			if (key_down)
			{
				result = true;
				if (Orientation::VERTICAL == orientation)
					updateValue(screenToWorld(event->motion.y - (pv->getRealY() + rect.y)));
				else if (Orientation::HORIZONTAL == orientation)
					updateValue(screenToWorld(event->motion.x - (pv->getRealX() + rect.x)));
			}
			break;
		case EVT_MOUSE_BTN_UP:
			key_down = false;
			break;
		default:
			break;
		}
		return result;
	}

	void draw() override
	{
		if (Orientation::HORIZONTAL == orientation)
		{
			SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
			fillRoundedRectF(renderer, rect, corner_radius, bg_color);
			SDL_SetRenderDrawColor(renderer, lvl_bar_color.r, lvl_bar_color.g, lvl_bar_color.b, lvl_bar_color.a);
			fillRoundedRectF(renderer, lvl_rect, corner_radius, lvl_bar_color);
		}
		else if (Orientation::VERTICAL == orientation)
		{
			SDL_SetRenderDrawColor(renderer, lvl_bar_color.r, lvl_bar_color.g, lvl_bar_color.b, lvl_bar_color.a);
			fillRoundedRectF(renderer, rect, corner_radius, lvl_bar_color);
			SDL_SetRenderDrawColor(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
			fillRoundedRectF(renderer, lvl_rect, corner_radius, bg_color);
		}

		if (show_knob)
		{
			knob.draw(renderer);
		}
	}

	Slider &registerOnValueUpdateCallback(std::function<void(Slider &)> onValUpdateClbk)
	{
		onValUpdate = onValUpdateClbk;
		return *this;
	}

	auto getCurrentValue() const
	{
		if (Orientation::VERTICAL == orientation)
			return max_val - (current_val - min_pad);
		return current_val - min_pad;
	}

	auto getMinValue()const { return min_val - min_pad; }
	auto getMaxValue()const { return max_val - max_pad; }
	auto isKeyDown()const noexcept { return key_down; }
	auto getLevelLength() const
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
		{
			lvl_rect.w = (current_val * ratio);
			knob.dest.x = lvl_rect.x + lvl_rect.w - (knob.dest.w / 2.f);
			knob.dest.y = rect.y + (rect.h / 2.f) - (knob.dest.h / 2.f) - 1.f;
		}
		else if (Orientation::VERTICAL == orientation)
		{
			lvl_rect.h = (current_val * ratio);
			knob.dest.x = (lvl_rect.x + (lvl_rect.w / 2.f)) - (knob.dest.w / 2.f);
			knob.dest.y = (lvl_rect.y + lvl_rect.h) - (knob.dest.w / 2.f);
		}
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

	void updatePosBy(float dx, float dy) override
	{
		rect.x += dx;
		rect.y += dy;
		lvl_rect.x += dx;
		lvl_rect.y += dy;
		knob.dest.x += dx;
		knob.dest.y += dy;
	}

private:
	bool pointInBound(const float &x, const float &y) const noexcept
	{
		if (Orientation::VERTICAL == orientation)
		{
			if ((x >= pv->getRealX() + knob.dest.x) && (x < pv->getRealX() + knob.dest.x + knob.dest.w) && (y >= pv->getRealY() + rect.y) && (y < pv->getRealY() + rect.y + rect.h))
				return true;
		}
		else if (Orientation::HORIZONTAL == orientation)
		{
			if ((x >= pv->getRealX() + rect.x) && (x < pv->getRealX() + rect.x + rect.w) && (y >= pv->getRealY() + knob.dest.y) && (y < pv->getRealY() + knob.dest.y + knob.dest.h))
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
	float corner_radius = 0.f;
	SDL_FRect rect, lvl_rect;
	bool show_knob = true, key_down = false;
	float ratio = 0.f;
	float length = 0.f;
	std::function<void(Slider &)> onValUpdate = nullptr;
};

/*
template <typename T>
	concept is_cellblock = requires(T _t) {
		//{_t.getMaxVerticalGrids()}->std::convertible_to<int>;
		_t.getMaxVerticalGrids();
		_t.getLowestBoundCell();
		_t.getCellSpacing();
		_t.getCellWidth();
	};*/

class CellBlock;
class Select;

class Cell : public Context, public IView
{
	/*
  public:
	struct State
	{
		SDL_Color bg_color = {50, 50, 50, 255};
		SDL_Color on_hover_bg_color = {0x00, 0x00, 0x00, 0x00};
		float corner_radius = 0.0f;
		IViewAttributes iv_attr;

		std::vector<TextBoxAttributes> tb_attr;
		std::vector<EditBoxAttributes> editb_attr;
		std::vector<ImageButtonAttributes> img_attr;
		std::vector<SliderAttributes> slider_attr;

		// Callbacks or other interactive properties
		std::function<void(Cell &)> customDrawCallback = nullptr;
		std::function<bool(Cell &)> customEventHandlerCallback = nullptr;
		std::function<void(Cell &, std::any _data)> onDataSetChanged = nullptr;
		//std::function<void(Cell &, FormData _data)> onFormSubmit = nullptr;
		std::function<void(Cell &)> onUpdateCallback = nullptr;

		// Any other data needed to fully describe a cell's content and appearance
		// bool needsRebuild = true; // If Cell UI needs full re-config from this data
	};
*/
private:
	// private helpers used by CellBlock
	bool isHeader = false;
	uint32_t num_vert_grids = 1;
	float org_mx = 0.f, org_my = 0.f, mx = 0.f, my = 0.f,
		  scroll_y = 0.f, max_scroll = 0.f, dy = 0.f;
	/*current finger*/
	v2d_generic<float> cf = {0.f, 0.f};
	/*previous finger*/
	v2d_generic<float> pf = {0.f, 0.f};

	bool finger_down = false;

	enum class ScrollAction
	{
		None,
		Up,
		Down,
		Left,
		Right
	};
	ScrollAction scrlAction;
	bool cellblock_parent = false;
private:
	inline void updatePosByInternal(float _dx, float _dy)
	{
		for (auto &imgBtn : imageButton)
			imgBtn.updatePosBy(_dx, _dy);
		for (auto &textBx : textBox)
			textBx.updatePosBy(_dx, _dy);
		for (auto &slider : sliders)
			slider.updatePosBy(_dx, _dy);
		for (auto &rtext : runningText)
			rtext.updatePosBy(_dx, _dy);
		for (auto &iview : iViews)
			iview->updatePosBy(_dx, _dy);
	}

	void internal_handle_motion()
	{
		// ACTION_UP
		if (cf.y < pf.y)
		{
			if (scroll_y + (pf.y - cf.y) < max_scroll + ph(5.f))
			{
				dy = 0.f - (pf.y - cf.y);
				scroll_y += std::fabs(dy);
				updatePosByInternal(0.f, dy);
				scrlAction = ScrollAction::Up;
			}
		}
		// ACTION_DOWN
		else if (pf.y < cf.y)
		{
			if (scroll_y - (cf.y - pf.y) > 0.f)
			{
				dy = (cf.y - pf.y);
				scroll_y -= dy;
				updatePosByInternal(0.f, dy);
				scrlAction = ScrollAction::Down;
			}
		}
		pf = cf;
		dy = 0.f;
	}

public:
	friend class CellBlock;
	using FormData = std::unordered_map<std::string, std::string>;

public:
	SharedTexture texture = nullptr;
	bool redraw = true;

public:
	uint64_t index = 0;
	SDL_Color bg_color = {0x00, 0x00, 0x00, 0x00};
	SDL_Color onHoverBgColor = {0x00, 0x00, 0x00, 0x00};
	float corner_radius = 0.f;
	std::any user_data, dataSetChangedData;
	std::function<void(Cell &)> customDrawCallback = nullptr;
	std::function<bool(Cell &)> customEventHandlerCallback = nullptr;
	std::function<void(Cell &, std::any _data)> onDataSetChanged = nullptr;
	std::function<void(Cell &, FormData _data)> onFormSubmit = nullptr;
	std::function<void(Cell &)> onUpdateCallback = nullptr;
	std::deque<TextBox> textBox;
	std::deque<EditBox> editBox;
	std::deque<RunningText> runningText;
	std::deque<Slider> sliders;
	std::deque<ToggleButton> togButton;
    std::deque<ImageButton> imageButton;
    std::deque<Scroll> scroll_bars;
	std::deque<IView *> iViews;
	// std::deque<std::shared_ptr<CellBlock>> blocks;
	std::vector<CellBlock> blocks;
	Scroll scroll{};
	bool selected = false;
	bool isHighlighted = false;
	bool highlightOnHover = false;
	bool ignoreTextEvents = true;
    bool has_scroll_bar=false;

	std::vector<Cell> header_footer{};
	std::vector<Select> select;

public:
	Cell &setContext(Context *context_) noexcept
	{
		Context::setContext(context_);
		Context::setView(this);
		return *this;
	}

	Cell& setUseScroll(bool use_scroll) {
		useScrollView = use_scroll;
		return *this;
	}
	/*
	Cell::State getState()
	{
		return Cell::State{};
	}

	Cell &BuildFromState(Cell::State &_cell_state)
	{
		return *this;
	}*/

	inline Cell &clear()
	{
		scroll_y = 0.f, max_scroll = 0.f, dy = 0.f;
		textBox.clear();
		editBox.clear();
		imageButton.clear();
		sliders.clear();
		//runningText.clear();
		return *this;
	}

	inline Cell &registerCustomDrawCallback(std::function<void(Cell &)> _customDrawCallback) noexcept
	{
		this->customDrawCallback = std::move(_customDrawCallback);
		return *this;
	}

	inline Cell &registerCustomEventHandlerCallback(std::function<bool(Cell &)> _customEventHandlerCallback) noexcept
	{
		this->customEventHandlerCallback = std::move(_customEventHandlerCallback);
		return *this;
	}

	inline Cell &registerOnDataSetChangedCallback(std::function<void(Cell &, std::any _data)> _onDataSetChanged) noexcept
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

	Cell &addView(IView *iview)
	{
		// iViews.emplace_back(iview);
		return *this;
	}

	Cell& shrinkToFit() {
		max_scroll = 0.f;
		for (auto& txt : textBox) {
			max_scroll = std::max(max_scroll, (txt.bounds.y + txt.bounds.h) - bounds.h);
		}

		for (auto& slider : sliders) {
			max_scroll = std::max(max_scroll, (slider.bounds.y + slider.bounds.h) - bounds.h);
		}

		for (auto& edit : editBox) {
			max_scroll = std::max(max_scroll, (edit.bounds.y + edit.bounds.h) - bounds.h);
		}

		for (auto& img : imageButton) {
			max_scroll = std::max(max_scroll, (img.bounds.y + img.bounds.h) - bounds.h);
		}

		for (auto& rt : runningText) {
			max_scroll = std::max(max_scroll, (rt.bounds.y + rt.bounds.h) - bounds.h);
		}

		for (auto& tb : togButton) {
			max_scroll = std::max(max_scroll, (tb.bounds.y + tb.bounds.h) - bounds.h);
		}
		return *this;
	}

	Cell& addHeaderOrFooter(SDL_FRect pbounds,SDL_Color bg={0,0,0,0}, float _corner_radius = 0.f)
	{
		header_footer.push_back({});
		auto& cell = header_footer.back();
		cell.setContext(this);
		cell.bounds = { pw(pbounds.x), ph(pbounds.y), pw(pbounds.w), ph(pbounds.h) };
		cell.bg_color = bg;
		cell.corner_radius = _corner_radius;
		//GLogger.Log(Logger::Level::Info, "Cell bounds rect{", bounds.x, bounds.y, bounds.w, bounds.h, "}");
		//GLogger.Log(Logger::Level::Info, "Footer Cell bounds rect{", cell.bounds.x, cell.bounds.y, cell.bounds.w, cell.bounds.h,"}");
		return header_footer.back();
	}

	Cell& addSelect(/*Select::Props sprops, std::vector<Select::Value> values*/)
	{
		/*sprops.rect = {
			/*bounds.x + * pv->to_cust(sprops.rect.x, bounds.w),
			/*bounds.y + * pv->to_cust(sprops.rect.y, bounds.h),
			pv->to_cust(sprops.rect.w, bounds.w),
			pv->to_cust(sprops.rect.h, bounds.h),
		};
		select.emplace_back().Build(this, sprops,values);
		max_scroll = std::max(max_scroll, (sprops.rect.y + sprops.rect.h) - bounds.h);
		redraw = true;*/
		return *this;
	}

	Cell& addToggleButton(ToggleButtonAttr togBtnAttr) {
		togBtnAttr.rect = {/*bounds.x + */ pv->to_cust(togBtnAttr.rect.x, bounds.w),
			/*bounds.y + */ pv->to_cust(togBtnAttr.rect.y, bounds.h),
			pv->to_cust(togBtnAttr.rect.w, bounds.w),
			pv->to_cust(togBtnAttr.rect.h, bounds.h) };
		togButton.emplace_back()
			.Build(this, togBtnAttr);
		//max_scroll = std::max(max_scroll, (togBtnAttr.rect.y + togBtnAttr.rect.h) - bounds.h);
		updateMaxScroll(togBtnAttr.rect.y + togBtnAttr.rect.h);
		// if (is_form and _TextBoxAttr.type="submit"){
		// textBox.back().setonsubmithandler }
		redraw = true;
		return *this;
	}

	Cell &addTextBox(TextBoxAttributes _TextBoxAttr) noexcept
	{
		_TextBoxAttr.rect = {/*bounds.x + */ pv->to_cust(_TextBoxAttr.rect.x, bounds.w),
							 /*bounds.y + */ pv->to_cust(_TextBoxAttr.rect.y, bounds.h),
							 pv->to_cust(_TextBoxAttr.rect.w, bounds.w),
							 pv->to_cust(_TextBoxAttr.rect.h, bounds.h)};
		textBox.emplace_back()
			.Build(this, _TextBoxAttr);
		//max_scroll = std::max(max_scroll, (_TextBoxAttr.rect.y + _TextBoxAttr.rect.h) - bounds.h);
		updateMaxScroll(_TextBoxAttr.rect.y + _TextBoxAttr.rect.h);
		// if (is_form and _TextBoxAttr.type="submit"){
		// textBox.back().setonsubmithandler }
		redraw = true;
		return *this;
	}

	Cell &addTextBoxFront(TextBoxAttributes _TextBoxAttr) noexcept
	{
		_TextBoxAttr.rect = {/*bounds.x + */ pv->to_cust(_TextBoxAttr.rect.x, bounds.w),
							 /*bounds.y + */ pv->to_cust(_TextBoxAttr.rect.y, bounds.h),
							 pv->to_cust(_TextBoxAttr.rect.w, bounds.w),
							 pv->to_cust(_TextBoxAttr.rect.h, bounds.h)};
		textBox.emplace_front()
			.Build(this, _TextBoxAttr);
		// if (is_form and _TextBoxAttr.type="submit"){
		// textBox.back().setonsubmithandler }
		redraw = true;
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
		redraw = true;
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

	Cell &addRunningText(RunningText::Attr _RTextAttr)
	{
		_RTextAttr.rect = {/*bounds.x + */ pv->to_cust(_RTextAttr.rect.x, bounds.w),
						   /*bounds.y + */ pv->to_cust(_RTextAttr.rect.y, bounds.h),
						   pv->to_cust(_RTextAttr.rect.w, bounds.w),
						   pv->to_cust(_RTextAttr.rect.h, bounds.h)};
		runningText.emplace_back()
			.Build(this, _RTextAttr);
		//max_scroll = std::max(max_scroll, (_RTextAttr.rect.y + _RTextAttr.rect.h) - bounds.h);
		updateMaxScroll(_RTextAttr.rect.y + _RTextAttr.rect.h);
		redraw = true;
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
			/*bounds.x + */ pv->to_cust(_EditBoxAttr.rect.x, bounds.w),
			/*bounds.y + */ pv->to_cust(_EditBoxAttr.rect.y, bounds.h),
			pv->to_cust(_EditBoxAttr.rect.w, bounds.w),
			pv->to_cust(_EditBoxAttr.rect.h, bounds.h)};
		//GLogger.Log(Logger::Level::Info,"Cell bounds rect{",bounds.x,bounds.y,bounds.w,bounds.h,"}");
		//GLogger.Log(Logger::Level::Info,"Edit bounds rect{",_EditBoxAttr.rect.x,_EditBoxAttr.rect.y,_EditBoxAttr.rect.w,_EditBoxAttr.rect.h,"}");
		editBox.emplace_back()
			.Build(this, _EditBoxAttr);
		//max_scroll = std::max(max_scroll, (_EditBoxAttr.rect.y + _EditBoxAttr.rect.h) - bounds.h);
		updateMaxScroll(_EditBoxAttr.rect.y + _EditBoxAttr.rect.h);
		redraw = true;
		return *this;
	}

	Cell &
	addEditBoxFront(EditBoxAttributes _EditBoxAttr)
	{
		_EditBoxAttr.rect = {
			/*bounds.x + */ pv->to_cust(_EditBoxAttr.rect.x, bounds.w),
			/*bounds.y + */ pv->to_cust(_EditBoxAttr.rect.y, bounds.h),
			pv->to_cust(_EditBoxAttr.rect.w, bounds.w),
			pv->to_cust(_EditBoxAttr.rect.h, bounds.h)};
		editBox.emplace_front()
			.Build(this, _EditBoxAttr);
		redraw = true;
		return *this;
	}

	/*Cell& addEditBoxVertArray(EditBoxAttributes _TextBoxAttr, float percentageMargin, std::vector<std::string> _texts)
		{
			const auto yStep = _TextBoxAttr.rect.h + percentageMargin;
			for (auto& _txt : _texts)
			{
				_TextBoxAttr.textAttributes.text = _txt;
				addTextBox(_TextBoxAttr);
				_TextBoxAttr.rect.y += yStep;
			}
			return *this;
		}*/

	/*	template <is_cellblock T>
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
*/

	Cell &addImageButton(ImageButtonAttributes imageButtonAttributes, const PixelSystem &pixel_system = PixelSystem::PERCENTAGE)
	{
		imageButton.emplace_back().setContext(this);
		if (pixel_system == PixelSystem::PERCENTAGE)
		{
			imageButtonAttributes.rect = SDL_FRect{
				/*bounds.x + */ pv->to_cust(imageButtonAttributes.rect.x, bounds.w),
				/*bounds.y + */ pv->to_cust(imageButtonAttributes.rect.y, bounds.h),
				pv->to_cust(imageButtonAttributes.rect.w, bounds.w),
				pv->to_cust(imageButtonAttributes.rect.h, bounds.h)};
			imageButton.back().Build(imageButtonAttributes);
			//max_scroll = std::max(max_scroll, (imageButtonAttributes.rect.y + imageButtonAttributes.rect.h) - bounds.h);
			updateMaxScroll(imageButtonAttributes.rect.y + imageButtonAttributes.rect.h);
		}
		else
		{
			imageButton.back().Build(imageButtonAttributes);
		}
		redraw = true;
		return *this;
	}

	Cell &addImageButtonFront(ImageButtonAttributes imageButtonAttributes, const PixelSystem &pixel_system = PixelSystem::PERCENTAGE)
	{
		imageButton.emplace_front().setContext(this);
		if (pixel_system == PixelSystem::PERCENTAGE)
		{
			imageButtonAttributes.rect = SDL_FRect{
				/*bounds.x + */ pv->to_cust(imageButtonAttributes.rect.x, bounds.w),
				/*bounds.y + */ pv->to_cust(imageButtonAttributes.rect.y, bounds.h),
				pv->to_cust(imageButtonAttributes.rect.w, bounds.w),
				pv->to_cust(imageButtonAttributes.rect.h, bounds.h)};

			imageButton.front().Build(imageButtonAttributes);
		}
		else
		{
			imageButton.front().Build(imageButtonAttributes);
		}
		redraw = true;
		return *this;
	}

	Cell &addSlider(SliderAttributes sAttr)
	{
		sAttr.rect = {
			pv->to_cust(sAttr.rect.x, bounds.w),
			pv->to_cust(sAttr.rect.y, bounds.h),
			pv->to_cust(sAttr.rect.w, bounds.w),
			pv->to_cust(sAttr.rect.h, bounds.h),
		};
		sliders.emplace_back().Build(this, sAttr);
		//max_scroll = std::max(max_scroll, (sAttr.rect.y + sAttr.rect.h) - bounds.h);
		updateMaxScroll(sAttr.rect.y + sAttr.rect.h);
		redraw = true;
		return *this;
	}

    Cell &addScrollBar(ScrollAttributes sAttr)
    {
        sAttr.rect = {
                pv->to_cust(sAttr.rect.x, bounds.w),
                pv->to_cust(sAttr.rect.y, bounds.h),
                pv->to_cust(sAttr.rect.w, bounds.w),
                pv->to_cust(sAttr.rect.h, bounds.h),
        };
        scroll_bars.emplace_back().Build(this, sAttr);
        updateMaxScroll(sAttr.rect.y + sAttr.rect.h);
        redraw = true;
        return *this;
    }

	inline bool isPointInBound(float x, float y) const noexcept
	{
		if (x > pv->getRealX() + bounds.x && x < (pv->getRealX() + bounds.x + bounds.w) && y > pv->getRealY() + bounds.y && y < pv->getRealY() + bounds.y + bounds.h)
			return true;

		return false;
	}

	inline void updatePosBy(float _dx, float _dy) override
	{
		bounds.x += _dx;
		bounds.y += _dy;
	}

	inline void notifyDataSetChanged(std::any _dataSetChangedData)
	{
		// add a check for empty _dataSetChangedData before storing
		// or even consider passing _dataSetChangedData directly to the onDataSetChanged() callback
		dataSetChangedData = std::move(_dataSetChangedData);
		if (onDataSetChanged != nullptr)
			onDataSetChanged(*this, dataSetChangedData);
	}

	bool handleEvent() override
	{
		bool result = false;
		if (customEventHandlerCallback != nullptr)
		{
			bool temp_res = customEventHandlerCallback(*this);
			// redraw =temp_res;
			if (prevent_default_behaviour)
			{
				// reset the default behaviour to false
				prevent_default_behaviour = false;
				return temp_res;
			}
		}

		if (hidden)
			return result;

		for (auto child : childViews)
		{
			if (child->handleEvent())
			{
				return true;
			}
		}

		if (useScrollView) {
			if (scrollView.handleEvent(*event, bounds)) {
				// map ScrollView offset -> cell scroll_y and translate children by the delta
				float newOffset = scrollView.getScrollOffset();
				float delta = newOffset - scrollViewPrevOffset;
				if (delta != 0.f) {
					// apply inverse delta to children so visual content moves up when offset increases
					updatePosByInternal(0.f, -delta);
					scrollViewPrevOffset = newOffset;
					redraw = true;
				}
				return true;
			}
		}

		// In Cell::handleEvent(...) where you process events for scrolling:
		/*if (useScrollView) {
			// First, let ScrollView handle pointer / thumb / wheel events.
			// If it handled and changed offset, reflect that into content.
			if (scrollView.handleEvent(*event, bounds)) {
				syncScrollFromView();
				return true; // event consumed by scrollbar
			}
		}*/

		if (event->type == EVT_WPSC)
		{
			bounds =
				{
					DisplayInfo::Get().toUpdatedWidth(bounds.x),
					DisplayInfo::Get().toUpdatedHeight(bounds.y),
					DisplayInfo::Get().toUpdatedWidth(bounds.w),
					DisplayInfo::Get().toUpdatedHeight(bounds.h),
				};
			return false;
		}
		if (isHidden())
			return result;
		if (event->type == EVT_FINGER_DOWN || event->type == EVT_FINGER_UP)
		{
			pf = cf = { event->tfinger.x * DisplayInfo::Get().RenderW,
					   event->tfinger.y * DisplayInfo::Get().RenderH };
			if (not isPointInBound(cf.x, cf.y))
			{
				return false;
			}
		}

		bool ft_handled = false;
		for (auto& ft : header_footer) {
			if (ft.handleEvent()) {
				result = true;
				ft_handled = true;
				redraw = true;
			}
		}
		if (not ft_handled) {
			for (auto& imgBtn : imageButton) {
				result |= imgBtn.handleEvent();
				if (result)break;
			}
			for (auto& textBx : textBox) {
				result |= textBx.handleEvent();
				if (result)break;
			}
			for (auto& _editBox : editBox) {
				result or_eq _editBox.handleEvent();
				if (result)break;
			}
			for (auto& _slider : sliders) {
				result or_eq _slider.handleEvent();
				if (result)break;
			}
			for (auto& rt : runningText) {
				result or_eq rt.handleEvent();
				if (result)break;
			}
			for (auto& tb : togButton) {
				result or_eq tb.handleEvent();
				if (result)break;
			}
			/*for (auto& sl : select)
				result or_eq sl.handleEvent();*/
		}


		if (event->type == EVT_FINGER_DOWN)
		{
			pf = cf = { event->tfinger.x * DisplayInfo::Get().RenderW,
					   event->tfinger.y * DisplayInfo::Get().RenderH };
			if (isPointInBound(cf.x, cf.y))
			{
				dy = 0.f;
				if (not ft_handled and not result) { finger_down = true; }
				result or_eq true;
			}
			else {
				if (auto_hide) {
					hide();
				}
			}
		}
		else if (event->type == EVT_FINGER_MOTION)
		{
			cf = { event->tfinger.x * DisplayInfo::Get().RenderW,
					  event->tfinger.y * DisplayInfo::Get().RenderH };
			if (finger_down and not result and max_scroll > 0.f)
			{
				internal_handle_motion();
				// --- Existing kinetic / manual scroll code branch ---
		// Wherever your code updates contentScrollY (dragging, fling, wheel handling outside ScrollView),
		// after you mutate contentScrollY, call:
				/*{
					// Example after contentScrollY += dy;
					// clamp contentScrollY to [0, max]
					float contentLength = std::max(bounds.h, bounds.h + max_scroll);
					contentScrollY = clampScroll(contentScrollY, 0.f, std::max(0.f, contentLength - bounds.h));
					// move children visually (existing behavior)
					updatePosByInternal(0.f, -(contentScrollY - previousContentScrollY));
					// now update the ScrollView to match the manual move
					syncScrollToView();
					previousContentScrollY = contentScrollY; // keep your existing state
				}*/
				result or_eq true;
				// movedDistance += {0.f, dy};
				/*
				if (SDL_fabsf(dy) < 2.f)
				{
					scrlAction = ScrollAction::None;
				}*/
				// GLogger.Log(Logger::Level::Debug, "internal handle motion");
			}
		}
		else if (event->type == EVT_MOUSE_MOTION)
		{
#ifdef _MSC_VER
			if (not cellblock_parent) {
				if (isPointInBound(event->motion.x, event->motion.y) and highlightOnHover)
				{
					isHighlighted = true;
					//GLogger.Log(Logger::Level::Debug, "highlight on hover event");
					redraw = true;
					result = true;
				}
				else {
					if (isHighlighted and highlightOnHover) {
						isHighlighted = false;
						redraw = true;
					}
				}
			}
#endif // _MSC_VER

		}
		else if (event->type == EVT_FINGER_UP)
		{
			finger_down = false;
			dy = 0.f;
		}

		if (redraw == false)
		{
			redraw = result;
			// if(result)GLogger.Log(Logger::Level::Info,"cell redraw in event");
		}

		// std::cout << ", w:" << bounds.w << ", h:" << bounds.h << std::endl;

		return result;
	}

	void onUpdate() override
	{
        for(auto* vw:childViews){
            vw->onUpdate();
        }

        scroll.onUpdate();

		for (auto& _editBox : editBox)
		{
			if (_editBox.isActive())
			{
				redraw = true;
				break;
			}
		}
		if (not runningText.empty()) { redraw = true; }

		for (auto &ft : header_footer) {
			ft.onUpdate();
			if (ft.redraw) {
				redraw = true;
			}
		}

		if (onUpdateCallback != nullptr)
		{
			onUpdateCallback(*this);
		}
	}

	void draw() override
	{
		if (isHidden())
			return;
		if (texture == nullptr)
		{
			texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
										  SDL_TEXTUREACCESS_TARGET, (int)bounds.w, (int)bounds.h);
			SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
			// tbuild = true;
		}
		if (redraw)
		{
			CacheRenderTarget crt_(renderer);
			SDL_SetRenderTarget(renderer, texture.get());
			RenderClear(renderer, 0, 0, 0, 0);
			// use custom draw fun if available
			// the default draw func has no ordering
			if (nullptr == customDrawCallback)
			{
				// GLogger.Log(Logger::Level::Info,"dstd");
				if (not highlightOnHover and not isHighlighted) {
					RenderClear(renderer, bg_color.r, bg_color.g, bg_color.b, bg_color.a);
				}
				if (highlightOnHover and isHighlighted) {
					RenderClear(renderer, onHoverBgColor.r, onHoverBgColor.g, onHoverBgColor.b, onHoverBgColor.a);
					//GLogger.Log(Logger::Level::Debug, "highlight on hover");
				}

				for (auto &imgBtn : imageButton)
					imgBtn.draw();
				for (auto &textBx : textBox)
					textBx.draw();
				for (auto &_editBox : editBox)
					_editBox.draw();
				for (auto &_rtext : runningText)
					_rtext.draw();
				for (auto &_slider : sliders)
					_slider.draw();
				for (auto& tb : togButton)
					tb.draw();
				for (auto& ft : header_footer)
					ft.draw();
				/*for (auto& sl : select)
					sl.draw();*/
			}
			else
			{
				// GLogger.Log(Logger::Level::Info,"cstd");
				customDrawCallback(*this);
			}

			/*if (useScrollView) {
				scrollView.draw(renderer);
			}*/
			redraw = false;
			crt_.release(renderer);
			transformToRoundedTexture(renderer, texture.get(), corner_radius);
		}

		RenderTexture(renderer, texture.get(), nullptr, &bounds);

		for (auto child : childViews)
			child->draw();
	}

private:
    inline void updateMaxScroll2(float val){
        max_scroll = std::max(max_scroll, val - bounds.h);
        if(not has_scroll_bar and val > bounds.h){
            ScrollAttributes sAttr;
            sAttr.rect={to_cust(97.f,bounds.w),0.f,to_cust(3.f,bounds.w), bounds.h};
            sAttr.bg_color={100, 100, 100, 130};
			sAttr.level_bar_color = { 200, 96, 33, 0xff };
			sAttr.start_val = bounds.h;
			sAttr.min_val = 0.f;
			sAttr.max_val = bounds.h;
			sAttr.orientation = Orientation::VERTICAL;
            scroll.Build(this, sAttr);
            has_scroll_bar=true;
        }
    }

	ScrollView scrollView{};
	bool useScrollView = false;
	float scrollViewPrevOffset = 0.f;

	// The cell keeps its own "content scroll" state (existing kinetic code likely updates this)
	float contentScrollY = 0.f;           // pixels scrolled (0 .. max_scroll)
	//float max_scroll = 0.f;               // existing maximum scroll value for content

	// Helper: clamp offset to valid range
	static inline float clampScroll(float v, float minV, float maxV) {
		return std::min(std::max(v, minV), maxV);
	}

	// Call when the content layout changes (e.g. updateMaxScroll). Ensures ScrollView sizes match.
	void ensureScrollViewSetup() {
		if (!useScrollView) return;
		float contentLength = std::max(bounds.h, bounds.h + max_scroll); // total content height
		scrollView.setContentLength(contentLength);
		scrollView.setViewportLength(bounds.h);
		// ensure scrollView offset and contentScrollY are in-range and consistent
		contentScrollY = clampScroll(contentScrollY, 0.f, std::max(0.f, contentLength - bounds.h));
		scrollView.setScrollOffset(contentScrollY);
		scrollViewPrevOffset = contentScrollY;
	}

	// Sync content -> ScrollView (call from your kinetic scroll code)
	void syncScrollToView() {
		if (!useScrollView) return;
		// Keep ScrollView thumb in sync with contentScrollY
		float contentLength = std::max(bounds.h, bounds.h + max_scroll);
		scrollView.setContentLength(contentLength);
		scrollView.setViewportLength(bounds.h);
		float clamped = clampScroll(contentScrollY, 0.f, std::max(0.f, contentLength - bounds.h));
		scrollView.setScrollOffset(clamped);
		scrollViewPrevOffset = clamped;
	}

	// Sync ScrollView -> content (call after ScrollView reports changes)
	void syncScrollFromView() {
		if (!useScrollView) return;
		float newOffset = scrollView.getScrollOffset();
		newOffset = clampScroll(newOffset, 0.f, std::max(0.f, std::max(bounds.h, bounds.h + max_scroll) - bounds.h));
		float delta = newOffset - scrollViewPrevOffset;
		if (delta != 0.f) {
			// when ScrollView moves down (offset increases), we need to move children up => negative
			updatePosByInternal(0.f, -delta); // reuse your existing function that shifts children
			contentScrollY = newOffset;
			scrollViewPrevOffset = newOffset;
			redraw = true; // ensure redraw (use your existing redraw flag)
		}
	}


	// Replace the existing updateMaxScroll(...) implementation in Cell with this (or augment it)
// This sets up a ScrollView when content exceeds the viewport.
	inline void updateMaxScroll(float val) {
		max_scroll = std::max(max_scroll, val - bounds.h);

		// If content exceeds the cell height, create/use a ScrollView at the right edge
		if (!useScrollView && val > bounds.h) {
			// Build a ScrollView track on the right side (4% width) — tweak as needed
			SDL_FRect svBounds = {
				bounds.w - to_cust(2.f, bounds.w),
				0.f,
				to_cust(2.f, bounds.w),
				bounds.h
			};
			scrollView.setBounds(svBounds);
			// content length = total content height (visible area + max_scroll)
			float contentLength = std::max(bounds.h, val);
			scrollView.setContentLength(contentLength);
			scrollView.setViewportLength(bounds.h);
			useScrollView = true;
			scrollViewPrevOffset = 0.f;
			has_scroll_bar = true; // keep existing flag to indicate scrollbar presence
		}
		else if (useScrollView) {
			// update ScrollView content size if needed
			float contentLength = std::max(bounds.h, val);
			scrollView.setContentLength(contentLength);
			scrollView.setViewportLength(bounds.h);
		}
	}

};




struct CellBlockProps
{
	SDL_FRect rect = {0.f, 0.f, 0.f, 0.f};
	Margin margin = {0.f, 0.f, 0.f, 0.f};
	float cornerRadius = 0.f;
	SDL_Color bgColor = {0, 0, 0, 0};
	float cellSpacingX = 1.f;
	float cellSpacingY = 1.f;
};

class CellBlock : public Context, public IView
{
public:
	uint32_t prevLineCount = 0, consumedCells = 0, lineCount = 0, numPrevLineCells = 0;
	float consumedWidth = 0.f;

public:
	const SDL_FRect &getBounds() const noexcept
	{
		return bounds;
	}

	SDL_FRect &getBounds()
	{
		return bounds;
	}

	const float &getCellWidth() const noexcept
	{
		return this->cellWidth;
	}

	const uint32_t &getMaxVerticalGrids() const noexcept
	{
		return numVerticalGrids;
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

	void eraseCell(std::size_t _cell_index)
	{
		toBeErasedCells.emplace_back(_cell_index);
	}

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

	auto getSelectedCellIndex() const
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
			dy = df, scrlAction = ScrollAction::Up;
		else
			dy = 0.f, scrlAction = ScrollAction::Down;
		this->bounds.h = height;
		this->margin.h += df;
		if (texture.get() != nullptr)
			texture.reset();
		CacheRenderTarget rTargetCache(renderer);
		texture = CreateUniqueTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_TARGET, (int)bounds.w, (int)bounds.h);
		SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
		SDL_SetRenderTarget(renderer, texture.get());
		RenderClear(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
		rTargetCache.release(renderer);
		return *this;
	}

	CellBlock &setCellSpacing(const float cell_spacing) noexcept
	{
		this->CellSpacing = cell_spacing;
		return *this;
	}

	CellBlock &onClick(std::function<void(Cell &)> on_cell_clicked_callback) noexcept
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

	std::size_t getSizeOfPreAddedCells() const
	{
		return sizeOfPreAddedCells;
	}

	CellBlock &clearAndReset()
	{
		update_top_and_bottom_cells();
		ANIM_ACTION_DN = false;
		ANIM_ACTION_UP = false;
		interpolated.stop();
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
		toBeErasedCells.clear();

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
			cells.back().cellblock_parent = true;
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
			cells.back().getView()->rel_x = bounds.x;
			cells.back().getView()->rel_y = bounds.y;
			cells.back().cellblock_parent = true;
			fillNewCellDataCallback(cells.back());
			visibleCells.emplace_back(&cells.back());
			scrlAction = ScrollAction::Up;
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

	std::optional<std::reference_wrapper<Cell>> getHeaderCell()
	{
		if (fillNewCellDataCallbackHeader and not cells.empty())
		{
			return cells.front();
		}
		else
		{
			// GLogger.Log(Logger::Level::Error, "CellBlock::getHeaderCell() failed. no header cell was found. call CellBlock::addHeaderCell() before CellBlock::getHeaderCell()");
			return {};
		}
	}

	// Note this method must be invoked before CellBlock::Build() and CellBlock::addCell
	CellBlock &addHeaderCell(std::function<void(Cell &)> newCellSetUpCallback)
	{
		fillNewCellDataCallbackHeader = newCellSetUpCallback;
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
			auto &new_cell = cells.emplace_back()
								 .setContext(this)
								 .setIndex(cells.size() - 1);
			new_cell.adaptiveVsync = &CellsAdaptiveVsync;
			new_cell.cellblock_parent = true;
			newCellSetUpCallback(new_cell);
			if (maxCells < cells.size())
				maxCells = cells.size();
			if (new_cell.bounds.y <= margin.h)
			{
				visibleCells.push_back(&cells.back());
			}
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
	CellBlock &setCellRect(Cell &_cell, uint32_t numVertGrids, const float h, const float margin_x = 0.f, const float margin_y = 0.f)
	{
		[[unlikely]] if (_cell.isHeader)
		{
			numVertGrids = numVerticalGrids;
		}
		numVertGrids = std::clamp(numVertGrids, uint32_t(0), numVerticalGrids);
		if (numVertGrids == numVerticalGrids)
			isFlexible = false;
		_cell.num_vert_grids = numVertGrids;
		_cell.org_mx = margin_x;
		_cell.org_my = margin_y;
		_cell.mx = margin_x;
		_cell.my = margin_y;
		if (getMaxVerticalGrids() - consumedCells < numVertGrids)
			lineCount++, _cell.bounds.y += margin_y;
		SDL_FRect lbc;
		if (not _cell.isHeader)
			lbc = getLowestBoundCell().bounds;
		else
			lbc = header_cell.bounds;
		if (cells.size() > 1)
		{
			const auto &bck = cells[cells.size() - 2].bounds;
			if (lbc.y + lbc.h < bck.y + bck.h)
				lbc = bck;
		}

		_cell.bounds.y += lbc.y;
		if (lineCount != prevLineCount)
			consumedCells = 0, _cell.bounds.y += lbc.h + CellSpacingY;
		_cell.bounds.x += CellSpacingX / 2.f;
		_cell.bounds.x += (consumedCells * (getCellWidth() + CellSpacingX)) + margin_x;
		_cell.bounds.w = (static_cast<float>(numVertGrids) * getCellWidth()) - CellSpacingX;
		_cell.bounds.w -= margin_x * 2.f;
		_cell.bounds.h = h - (margin_y * 2.f);
		prevLineCount = lineCount;
		consumedCells += numVertGrids;
		[[unlikely]] if (_cell.isHeader)
		{
			_cell.bounds.x += bounds.x;
			_cell.bounds.y += bounds.y;
		}
		return *this;
	}

	float sumGainedGrids = 0.f;

	void handleFlexResize(float dw)
	{
		if (dw == 0.f)
			return;
		if (max_vert_grids <= 1)
		{
			cellWidth = margin.w / static_cast<float>(numVerticalGrids);
			allCellsHandleEvent();
			return;
		}

		// check if we lost or gained width
		if (dw < 0.f)
		{
			// handle lost grids
			const float lostGrids = std::fabs(dw) / cellWidth;
			const float fLostGrids = std::floor(lostGrids);
			std::cout << "LostGrids:" << lostGrids << "\n";
			std::cout << "PrevCellSpacingX:" << CellSpacingX << "\n";
			if (lostGrids >= 1.0f)
			{
				if ((float)(numVerticalGrids)-fLostGrids >= 1.f)
				{
					numVerticalGrids -= (uint32_t)fLostGrids;
					CellSpacingX += std::fabs(fLostGrids * cellWidth) / (float)(numVerticalGrids + 1);
					if (lostGrids > fLostGrids)
					{
						const float dx = ((lostGrids - fLostGrids) * cellWidth) / (floor)(numVerticalGrids + 1);
						if (CellSpacingX - dx >= 0.0)
						{
							CellSpacingX += dx;
						}
						else
						{
							if (numVerticalGrids - 1.f >= 1.f)
							{
								--numVerticalGrids;
								CellSpacingX += cellWidth / (float)(numVerticalGrids + 1);
							}
						}
					}
				}
				else
				{
					return;
				}
			}
			else
			{
				const float dx = (lostGrids * cellWidth) / (floor)(numVerticalGrids + 1);
				if (CellSpacingX - dx >= 0.0f)
				{
					CellSpacingX -= dx;
				}
				else
				{
					if (numVerticalGrids - 1 >= 1)
					{
						--numVerticalGrids;
						CellSpacingX += cellWidth / (float)(numVerticalGrids + 1);
						CellSpacingX -= dx;
					}
					else
					{
						std::cout << "can't shrink grids any more\n";
						return;
					}
				}
			}
		}
		else if (dw > 0.f)
		{
			bool reg = false;

			// redo_gained:
			const float gainedGrids = dw / cellWidth;
			const float fGainedGrids = std::floor(gainedGrids);
			sumGainedGrids += gainedGrids;
			std::cout << "ReGained:" << reg << "\n";
			std::cout << "GainedGrids:" << gainedGrids << "\n";
			std::cout << "PrevCellSpacingX:" << CellSpacingX << "\n";
			std::cout << "sumGainedGrids:" << sumGainedGrids << "\n";
			if (sumGainedGrids >= 1.f)
			{
				numVerticalGrids += (uint32_t)std::floor(sumGainedGrids);
				CellSpacingX = OrgCellSpacingX;
				if (sumGainedGrids > std::floor(sumGainedGrids))
				{
					const float dx = ((sumGainedGrids - std::floor(sumGainedGrids)) * cellWidth) / (floor)(numVerticalGrids + 1);
					CellSpacingX += dx;
				}
				sumGainedGrids = 0.f;
			}
			else
			{
				const float dx = dw / (floor)(numVerticalGrids + 1);
				CellSpacingX += dx;
			}

			/*if (gainedGrids >= 1.0f) {
					numVerticalGrids += (uint32_t)fGainedGrids;
					if (gainedGrids > fGainedGrids) {
						const float dx = ((gainedGrids - fGainedGrids) * cellWidth) / (floor)(numVerticalGrids + 1);
						CellSpacingX += dx;
					}
				}
				else {
					sumGainedGrids += gainedGrids;
					if (sumGainedGrids >= 1.f) {
						dw = sumGainedGrids * cellWidth;
						sumGainedGrids = 0.f;
						reg = true;
						CellSpacingX = OrgCellSpacingX*2.f;
						goto redo_gained;
					}
					else {
						CellSpacingX += dw / (floor)(numVerticalGrids+1);
					}
				}*/
		}
		std::cout << "CellSpacingX:" << CellSpacingX << "\n";
		clearAndReset();

		/*
		 * std::for_each(cells.begin(), cells.end(), [this](Cell& cell) { setCellRect(cell, cell.num_vert_grids, cell.bounds.h, CellSpacingX); });
		 */
	}

	void resetTexture()
	{
		CacheRenderTarget crt_(renderer);
		texture.reset();
		texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
									  SDL_TEXTUREACCESS_TARGET, (int)margin.w, (int)margin.h);
		SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
		SDL_SetRenderTarget(renderer, texture.get());
		RenderClear(renderer, 0, 0, 0, 0);
		// fillRoundedRectF(renderer, { 0.f,0.f,rect.w,rect.h }, cornerRadius, bgColor);
		crt_.release(renderer);
	}

	///  todo: build from view
	virtual void attachRelativeView(IView *_prev_view, float _margin = 0.f) override
	{
		bounds.y = _prev_view->bounds.y + _prev_view->bounds.h + _margin;
		margin.y += _prev_view->bounds.y + _prev_view->bounds.h + _margin;
	}

	CellBlock &Build(Context *context_, const int &maxCells_, const int &_numVerticalGrids, const CellBlockProps &_blockProps, const ScrollDirection &scroll_direction = ScrollDirection::VERTICAL)
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
		numVerticalGrids = _numVerticalGrids;
		max_vert_grids = _numVerticalGrids;
		CellSpacingX = _blockProps.cellSpacingX;
		OrgCellSpacingX = _blockProps.cellSpacingX;
		CellSpacingY = _blockProps.cellSpacingY;
		cellWidth = ((margin.w - (_blockProps.cellSpacingX * (_numVerticalGrids - 1))) / static_cast<float>(_numVerticalGrids));

		if (fillNewCellDataCallbackHeader)
		{
			update_top_and_bottom_cells();
			header_cell = Cell{};
			header_cell.isHeader = true;
			header_cell.cellblock_parent = true;
			header_cell.setContext(this)
				.setIndex(0);
			header_cell.pv = this;
			header_cell.adaptiveVsync = adaptiveVsync;
			fillNewCellDataCallbackHeader(header_cell);
			header_h = header_cell.bounds.h;
			margin.y += header_h + 2.f;
			margin.h -= header_h + 2.f;
			// GLogger.Log(Logger::Level::Info, "CellBlock::addHeaderCell() Success Index:" + std::to_string(header_cell.index));
		}

		if (texture != nullptr)
		{
			texture.reset();
			texture = nullptr;
		}
		CacheRenderTarget crt_(renderer);
		texture = CreateSharedTexture(renderer, SDL_PIXELFORMAT_RGBA8888,
									  SDL_TEXTUREACCESS_TARGET, (int)margin.w, (int)margin.h);
		SDL_SetTextureBlendMode(texture.get(), SDL_BLENDMODE_BLEND);
		SDL_SetRenderTarget(renderer, texture.get());
		// RenderClear(renderer, 0, 0, 0, 0);
		RenderClear(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
		//		fillRoundedRectF(renderer, { 0.f,0.f,rect.w,rect.h }, 0.f, bgColor);
		// fillRoundedRectF(renderer, {0.f, 0.f, bounds.w, bounds.h}, 0.f, bgColor);
		/*if (fillNewCellDataCallbackHeader)
			header_cell.draw();*/
		crt_.release(renderer);

		const int originalMaxCells = maxCells;
		for (auto preAddedCellSetUpCallback : preAddedCellsSetUpCallbacks)
		{
			update_top_and_bottom_cells();
			cells.emplace_back()
				.setContext(this)
				.setIndex(cells.size() - 1)
				.adaptiveVsync = adaptiveVsync;
			cells.back().cellblock_parent = true;
			preAddedCellSetUpCallback(cells.back());
			maxCells++;
			visibleCells.push_back(&cells.back());
			++backIndex;
			update_top_and_bottom_cells();
		}

		if (originalMaxCells > 0)
		{
			cells.emplace_back()
				.setContext(this)
				.setIndex(cells.size() - 1)
				.bg_color = cell_bg_color;
			cells.back().pv = this;
			cells.back().getView()->rel_x = bounds.x;
			cells.back().getView()->rel_y = bounds.y;
			cells.back().adaptiveVsync = adaptiveVsync;
			cells.back().cellblock_parent;
			fillNewCellDataCallback(cells.back());
			visibleCells.emplace_back(&cells.back());

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
					cells.back().getView()->rel_x = bounds.x;
					cells.back().getView()->rel_y = bounds.y;
					cells.back().cellblock_parent = true;
					fillNewCellDataCallback(cells.back());
				}
				++backIndex;
				visibleCells.push_back(&cells[backIndex]);
			}
			while (visibleCells[0]->bounds.y + visibleCells[0]->bounds.h < 0.f)
				visibleCells.pop_front(), frontIndex = visibleCells.front()->index;

			scrlAction = ScrollAction::Up;
		}
		else
		{
			SDL_Log("empty cellblock");
		}
		transformToRoundedTexture(renderer, texture.get(), cornerRadius);

		BuildWasCalled = true;
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
		{
			return result;
		}
		for (auto child : childViews)
		{
			if (child->handleEvent())
			{
				updateHighlightedCell(-1);
				SIMPLE_RE_DRAW = true;
				return true;
			}
		}
		if (fillNewCellDataCallbackHeader)
			header_cell.handleEvent();
		if (event->type == EVT_RENDER_TARGETS_RESET)
		{
			SDL_Log("targets reset. must recreate textures");
			allCellsHandleEvent();
			SIMPLE_RE_DRAW = true;
		}
		else if (event->type == EVT_WPSC)
		{
			float dw = bounds.w;
			bounds =
				{
					DisplayInfo::Get().toUpdatedWidth(bounds.x),
					DisplayInfo::Get().toUpdatedHeight(bounds.y),
					DisplayInfo::Get().toUpdatedWidth(bounds.w),
					DisplayInfo::Get().toUpdatedHeight(bounds.h),
				};
			dw = bounds.w - dw;
			margin =
				{
					DisplayInfo::Get().toUpdatedWidth(margin.x),
					DisplayInfo::Get().toUpdatedHeight(margin.y),
					DisplayInfo::Get().toUpdatedWidth(margin.w),
					DisplayInfo::Get().toUpdatedHeight(margin.h),
				};
			resetTexture();
			// handleFlexResize(dw);
			allCellsHandleEvent();
			SIMPLE_RE_DRAW = true;
		}
		else if (event->type == EVT_FINGER_DOWN)
		{
			pf = cf = {event->tfinger.x * DisplayInfo::Get().RenderW,
					   event->tfinger.y * DisplayInfo::Get().RenderH};
			if (isPosInbound(cf.x, cf.y))
			{
				dy = 0.f;
				dc = 0.f;
				//	SDL_Log("wa");
				for (auto child : childViews)
				{
					if (not child->hidden)
					{
						child->hide();
						result = true;
						// SDL_Log("a");
					}
				}
				if (result)
					return result;
				SIMPLE_RE_DRAW = true;
				KEYDOWN = true, interpolated.stop();
				visibleCellsHandleEvent();
				result or_eq true;
			}
			FingerMotion_TM = SDL_GetTicks();
		}
		/*else if (event->type == SDL_MOUSEWHEEL)
			{
				MOTION_OCCURED = true;
				visibleCellsHandleEvent();
				if (isPosInbound(event->wheel.mouseX, event->wheel.mouseY) and maxCells > 0)
				{
					pf = {event->wheel.mouseX,
						  event->wheel.mouseY};
					cf = {pf.x + (event->wheel.x * 40.f),
						  pf.y + (event->wheel.y * 40.f)};
					movedDistance += {event->wheel.x * 5.f, event->wheel.y * 5.f};
					internal_handle_motion();
					updateHighlightedCell(-1);
					FingerUP_TM = SDL_GetTicks();
					return true;
				}
				FingerUP_TM = SDL_GetTicks();
			}*/
		else if (event->type == EVT_FINGER_MOTION)
		{
			MOTION_OCCURED = true;
			// visibleCellsHandleEvent();
			if (KEYDOWN and maxCells > 0)
			{
				cf = {event->tfinger.x * DisplayInfo::Get().RenderW,
					  event->tfinger.y * DisplayInfo::Get().RenderH};
				/*dc += event->tfinger.dy  * DisplayInfo::Get().RenderH;
				if (std::fabs(dc) < 4.f)
				{
					skipFrame = true;
					return true;
				}
				dc = 0.f;*/
				internal_handle_motion();
				updateHighlightedCell(-1);
				movedDistance += {0.f, dy};
				if (SDL_fabsf(dy) < 2.f)
				{
					scrlAction = ScrollAction::None;
				}
				else
					FingerUP_TM = SDL_GetTicks();
				return true;
			}
			FingerUP_TM = SDL_GetTicks();
		} 
		else if (event->type == EVT_MOUSE_MOTION)
		{
#ifdef _MSC_VER
			if (not KEYDOWN)
			{
				// current finger
				cf = {(float)event->motion.x, (float)event->motion.y};
				// current finger transformed
				const v2d_generic<float> cf_trans = {cf.x - margin.x, cf.y - margin.y};
				bool cellFound = false;
				if (isPosInbound(event->motion.x, event->motion.y))
				{
					result = true;
					auto fc = std::find_if(visibleCells.begin(), visibleCells.end(),
										   [this, &cf_trans, &cellFound](Cell *cell) {
											   if (cf_trans.y >= cell->bounds.y and
												   cf_trans.y <= cell->bounds.y + cell->bounds.h and
												   cf_trans.x >= cell->bounds.x and
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
#endif // _MSC_VER
		}
		else if (event->type == EVT_FINGER_UP)
		{
			////to do: should not use the time processed by the application
			// to measure the time between events instead use event.time
			const auto finger_up_dt = (SDL_GetTicks() - FingerUP_TM);
			interpolated.stop();
			dc = 0.f;
			// SDL_Log("f∆");
			if (KEYDOWN and not MOTION_OCCURED)
			{
				cf = {event->tfinger.x * DisplayInfo::Get().RenderW,
					  event->tfinger.y * DisplayInfo::Get().RenderH};
				// const v2d_generic<float> cf_trans = {cf.x - margin.x, cf.y - margin.y};
				//	SDL_Log("s1");
				if (isPosInbound(cf.x, cf.y))
				{
					result = true;
					// SDL_Log("s2");
					auto fc = std::find_if(visibleCells.begin(), visibleCells.end(),
										   [this](Cell *cell)
										   {
											   // SDL_Log("s3:%f,%f,%f,%f",cell->bounds.x,cell->bounds.y,cell->bounds.w,cell->bounds.h);
											   if (/*cf_trans.y >= cell->bounds.y and
												   cf_trans.y <= cell->bounds.y + cell->bounds.h and
												   cf_trans.x >= cell->bounds.x and
												   cf_trans.x <= cell->bounds.x + cell->bounds.w*/
												   cell->isPointInBound(cf.x, cf.y))
											   {
												   // SDL_Log("z");
												   updateSelectedCell(cell->index);
												   if (not cell->handleEvent())
												   {
													   CELL_PRESSED = true;
													   pressed_cell_index = cell->index;
													   // SDL_Log("CLKD_CELL: %d", pressed_cell_index);
													   updateHighlightedCell(-1);
													   // cell->redraw = true;
												   }
												   return true;
											   }
											   return false;
										   });
				}
			}

			if (MOTION_OCCURED and isPosInbound(cf.x, cf.y) and isBlockScrollable())
			{
				result = true;
				// SDL_Log("FU_TM: %d", finger_up_dt);
				float dt = static_cast<float>((FingerUP_TM - FingerMotion_TM));

				if (finger_up_dt > 100)
					dt = 30000.f, movedDistance.y = 0.01f;

				if (dt <= 0.f)
					dt = 0.000001f;
				auto speed = ((SDL_fabsf(movedDistance.y) / dt)*15);
				if (speed <= 0.f)
					speed = 0.000001f;
				// SDL_Log("Speed: %f", speed);
				/*if (finger_up_dt > 45)
					speed *= 10.f;*/
				// SDL_Log("moved distance: %f DT: %f  speed: %f", fabs(movedDistance.y), dt, speed);
				if (movedDistance.y >= 0.f)
					ANIM_ACTION_DN = true;
				else
					ANIM_ACTION_UP = true;
				/*
				if (speed > 0)
					scroll_step = (float)speed / 4.f;
				else
					scroll_step = 10.f;*/
				/*interpolated.setTransitionFunction(TransitionFunction::EaseOutQuad);
				interpolated.setValue(speed);
				interpolated.setDuration(speed / 8.f);*/

				// compute a sane fling magnitude
				const float minFling = 1.f;
				const float maxFling = 5000.f;
				float fling = std::clamp(speed, minFling, maxFling);

				// map fling magnitude to a reasonable duration (seconds) and clamp
				// (higher fling -> slightly longer duration, but capped)
				const float duration = std::clamp(fling / 100.0f, 0.05f, 4.0f);

				interpolated.setTransitionFunction(TransitionFunction::EaseOutQuad);
				interpolated.setValue(fling);
				interpolated.setDuration(duration);

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

		if (CELL_PRESSED or SIMPLE_RE_DRAW or !interpolated.isDone() or CellsAdaptiveVsync.hasRequests())
		{
			adaptiveVsyncHD.startRedrawSession();
			dy = 0.f;
		}

		if (not toBeErasedCells.empty())
			SIMPLE_RE_DRAW = false;
		return result;
	}

	void onUpdate() override
	{
		if (fillNewCellDataCallbackHeader)
			header_cell.onUpdate();
		for (auto &cell : visibleCells)
		{
			cell->onUpdate();
			// SDL_Log("s9:%f,%f,%f,%f",cell->bounds.x,cell->bounds.y,cell->bounds.w,cell->bounds.h);
		}
	}

	void scroll(float distance)
	{
		if (distance >= 0.f)
			ANIM_ACTION_DN = true;
		else
			ANIM_ACTION_UP = true;
		SIMPLE_RE_DRAW = false;
		// scrollAnimInterpolator.startWithDistance(distance);
		// adaptiveVsyncHD.startRedrawSession();
	}

	// BUG: infinite recursion if you invoke this func inside a cell pressed callback
	void draw() override
	{
		if (not enabled or isHidden())
			return;
		//	update_cells_with_async_images();
		/*if (label == "inner cell block") {
			GLogger.Log(Logger::Level::Info, "drawing [test inner-view shown] label:");
		}*/
		if (SIMPLE_RE_DRAW)
		{
			SIMPLE_RE_DRAW = false;
			goto simple_rdraw;
			// simpleDraw();
		}
		if (scrlAction == ScrollAction::None and not ANIM_ACTION_DN and not ANIM_ACTION_UP and not CELL_PRESSED and not adaptiveVsyncHD.shouldReDrawFrame())
		{
			// fillRoundedRectF(renderer, {bounds.x, bounds.y, bounds.w, bounds.h}, cornerRadius, bgColor);
			
			// transformToRoundedTexture(renderer, texture.get(), cornerRadius);
			RenderTexture(renderer, texture.get(), nullptr, &margin);
			if (fillNewCellDataCallbackHeader)
				header_cell.draw();
			if (not childViews.empty())
			{
				for (auto child : childViews)
					child->draw();
			}
			adaptiveVsyncHD.stopRedrawSession();
			return;
		}

		if (!interpolated.isDone())
		{
			if (ANIM_ACTION_DN)
			{
				dy = interpolated.getValue();
				dy *= scroll_step;
				scrlAction = ScrollAction::Down;
				if (cells.front().bounds.y + dy > 0.f)
				{
					if (cells.front().bounds.y > 0.f)
						dy = 0.f - (cells.front().bounds.y);
					else
						dy = std::fabs(cells.front().bounds.y);
					interpolated.stop();
				}
			}
			else if (ANIM_ACTION_UP)
			{
				dy = 0.f - interpolated.getValue();
				dy *= scroll_step;
				scrlAction = ScrollAction::Up;
				if (cells.back().bounds.y + cells.back().bounds.h + dy < margin.h and
					cells.back().index >= (maxCells - 1))
				{
					if (cells.back().bounds.y + cells.back().bounds.h < margin.h)
					{
						dy = margin.h - cells.back().bounds.y - cells.back().bounds.h;
					}
					else
					{
						dy = cells.back().bounds.y + cells.back().bounds.h - margin.h;
						// std::cout << "mh:" << margin.h << " "<< "cy:" << cells.back().bounds.y + cells.back().bounds.h << "dy:" << dy << ", ";
					}
					interpolated.stop();
				}
			}
		}
		else
			ANIM_ACTION_DN = ANIM_ACTION_UP = false;

		if (not cells.empty())
		{
			if (dy not_eq 0.f and SDL_fabsf(dy) > 1.5f)
			{
				movedDistanceSinceStart.y += dy;
				// std::for_each(cells.begin(), cells.end(), [this](Cell<CustomCellData>& cell) {Async::GThreadPool.enqueue(&Cell<CustomCellData>::updatePosBy, &cell,0.f, dy); });
				std::for_each(cells.begin(), cells.end(), [this](Cell &cell)
							  { cell.updatePosBy(0.f, dy); });
			}
			if (CELL_PRESSED)
			{
				// SDL_Log("CLKD_CELL: %d", pressed_cell_index);
				CELL_PRESSED = false;
				if (onCellClickedCallback)
					onCellClickedCallback(cells[pressed_cell_index]);
				pressed_cell_index = 0;
			}

			if (cells.back().index >= maxCells - 1)
				FIRST_LOAD = false;

			switch (scrlAction)
			{
			case ScrollAction::Up:
			{
				// auto beg = std::chrono::steady_clock::now();
				scrollUp();
				break;
			}
			case ScrollAction::Down:
			{
				// auto beg = std::chrono::steady_clock::now();
				scrollDown();
				break;
			}
			case ScrollAction::None:
				break;
			}

			dy = 0.f;
			scrlAction = ScrollAction::None;
		}
	simple_rdraw:
		simpleDraw();
		//		transformToRoundedTexture(renderer, texture.get(), cornerRadius);

		if (not toBeErasedCells.empty())
		{
			// SDL_Log("@");
			for (auto i : toBeErasedCells)
			{
				cells.erase(cells.begin() + i);
			}
			updateHighlightedCell(-1);
			updateSelectedCell(0);
			maxCells -= toBeErasedCells.size();
			toBeErasedCells.clear();
		}
	}

protected:
	void simpleDraw()
	{
		// fillRoundedRectF(renderer, {bounds.x, bounds.y, bounds.w, bounds.h}, cornerRadius, bgColor);
		/*if (fillNewCellDataCallbackHeader)
			header_cell.draw();*/
		CacheRenderTarget crt_(renderer);
		SDL_SetRenderTarget(renderer, texture.get());
		RenderClear(renderer, bgColor.r, bgColor.g, bgColor.b, bgColor.a);
		// fillRoundedRectF(renderer, {0.f, 0.f, bounds.w, bounds.h}, 0.f, bgColor);
		for (auto &cell : visibleCells)
		{
			cell->draw();
			// SDL_Log("s9:%f,%f,%f,%f",cell->bounds.x,cell->bounds.y,cell->bounds.w,cell->bounds.h);
		}
		
		crt_.release(renderer);
		transformToRoundedTexture(renderer, texture.get(), cornerRadius);
		RenderTexture(renderer, texture.get(), nullptr, &margin);
		if (not childViews.empty())
		{
			for (auto child : childViews)
				child->draw();
		}
		adaptiveVsyncHD.stopRedrawSession();
		if (CellsAdaptiveVsync.hasRequests() or !interpolated.isDone())
		{
			adaptiveVsyncHD.startRedrawSession();
		}
	}

	void scrollUp()
	{
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
				cells.back().getView()->rel_x = bounds.x;
				cells.back().getView()->rel_y = bounds.y;
				fillNewCellDataCallback(cells.back());
			}
			++backIndex;
			visibleCells.push_back(&cells[backIndex]);
		}
		while (visibleCells[0]->bounds.y + visibleCells[0]->bounds.h < 0.f)
			visibleCells.pop_front(), frontIndex = visibleCells.front()->index;
	}

	void scrollDown()
	{
		while (visibleCells[0]->index > cells.front().index and visibleCells[0]->bounds.y + visibleCells[0]->bounds.h > 0.f)
		{
			--frontIndex;
			visibleCells.push_front(&cells[frontIndex]);
			update_top_and_bottom_cells();
		}
		while (visibleCells.back()->bounds.y > margin.h)
			visibleCells.pop_back(), backIndex = visibleCells.back()->index, update_top_and_bottom_cells(); /*, SDL_Log("back pop rs: %d", cells_tmp.size())*/
	}

	void update_cells_with_async_images()
	{
		if (cellsWithAsyncImages.empty())
			return;
		std::erase_if(cellsWithAsyncImages, [&](ImageButton *_imgBtn)
					  {
					if (_imgBtn->isAsyncLoadComplete()) { _imgBtn->draw(); return true; }
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
		{
			cells[PREV_SELECTED_CELL].selected = false;
			cells[PREV_SELECTED_CELL].redraw = true;
		}
		cells[SELECTED_CELL].selected = true;
		cells[SELECTED_CELL].redraw = true;
	}

	void updateHighlightedCell(int64_t _highlightedCell)
	{
		// clear the prev highlighted cell
		if (HIGHLIGHTED_CELL >= 0)
		{
			if (HIGHLIGHTED_CELL < cells.size())
			{
				cells[HIGHLIGHTED_CELL].isHighlighted = false;
				cells[HIGHLIGHTED_CELL].redraw = true;
			}
		}
		// PREV_HIGHLIGHTED_CELL = HIGHLIGHTED_CELL;
		HIGHLIGHTED_CELL = _highlightedCell;
		// if (PREV_HIGHLIGHTED_CELL >= 0)cells[PREV_HIGHLIGHTED_CELL].isHighlighted = false;
		if (HIGHLIGHTED_CELL >= 0)
		{
			cells[HIGHLIGHTED_CELL].isHighlighted = true;
			cells[HIGHLIGHTED_CELL].redraw = true;
		}
	}

	bool visibleCellsHandleEvent()
	{
		bool result = false;
		// SDL_Log("alsaa");
		for (auto &cell : visibleCells)
		{
			// SDL_Log("sm0:%f,%f,%f,%f",cell->bounds.x,cell->bounds.y,cell->bounds.w,cell->bounds.h);
			cell->handleEvent();
			// SDL_Log("sm1:%f,%f,%f,%f",cell->bounds.x,cell->bounds.y,cell->bounds.w,cell->bounds.h);
		}
		return result;
	}

	bool allCellsHandleEvent()
	{
		bool result = false;
		if (fillNewCellDataCallbackHeader)
			header_cell.handleEvent();
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
				scrlAction = ScrollAction::Up;
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
			else
			{
				dy += (cf.y - pf.y);
				if (cells.front().bounds.y + dy > 0 && cells.front().index <= 0)
					dy = SDL_fabsf(cells.front().bounds.y);
				scrlAction = ScrollAction::Down;
			}
		}
		pf = cf;
		// dy *= 2.f;
	}

private:
	enum class ScrollAction
	{
		None,
		Up,
		Down,
		Left,
		Right
	};
	ScrollAction scrlAction;

private:
	std::function<void(Cell &)> onCellClickedCallback = nullptr;
	std::function<void(Cell &)> fillNewCellDataCallback = nullptr;
	std::function<void(Cell &)> fillNewCellDataCallbackHeader = nullptr;
	std::deque<Cell> cells;
	std::deque<Cell *> visibleCells;
	std::deque<ImageButton *> cellsWithAsyncImages;
	std::deque<std::function<void(Cell &)>> preAddedCellsSetUpCallbacks;
	std::vector<std::size_t> toBeErasedCells;
	Cell header_cell, footer_cell;
	AdaptiveVsyncHandler adaptiveVsyncHD;
	AdaptiveVsync CellsAdaptiveVsync;
	SDL_FRect margin;
	SharedTexture texture = nullptr;
	SDL_Color cell_bg_color = {0x00, 0x00, 0x00, 0x00};
	uint64_t maxCells = 0;
	// SDL_FRect dest;
	float cellWidth = 0.f, CellSpacing = 0.f, CellSpacingX = 1.f, OrgCellSpacingX = 0.f, CellSpacingY = 0.f;
	float dy = 0.f, dc = 0.f, scroll_step = 2.f;
	float cornerRadius = 0.f;
	float header_h = 0.f;
	/*current finger*/
	v2d_generic<float> cf = {0.f, 0.f};
	/*previous finger*/
	v2d_generic<float> pf = {0.f, 0.f};
	v2d_generic<float> movedDistance = {0.f, 0.f};
	v2d_generic<float> movedDistanceSinceStart = {0.f, 0.f};
	uint32_t numVerticalGrids = 0;
	uint32_t FingerMotion_TM = 0;
	uint32_t FingerUP_TM = 0;
	uint32_t max_vert_grids = 1;
	SDL_Color bgColor = {0, 0, 0, 0xff};
	std::size_t backIndex = 0, frontIndex = 0;
	std::size_t topCell = 0, bottomCell = 0;
	bool enabled = true, CELL_PRESSED = false, CELL_HIGHLIGHTED = false, KEYDOWN = false;
	bool MOTION_OCCURED = false, ANIM_ACTION_UP = false, ANIM_ACTION_DN = false, FIRST_LOAD = true;
	bool SIMPLE_RE_DRAW = false;
	bool BuildWasCalled = false;
	bool isFlexible = true;
	int64_t SELECTED_CELL = 0 - 1, PREV_SELECTED_CELL = 0 - 1;
	int64_t HIGHLIGHTED_CELL = 0 - 1, PREV_HIGHLIGHTED_CELL = 0 - 1;
	std::size_t sizeOfPreAddedCells = 0;
	std::size_t pressed_cell_index = 0;
	Interpolated<float> interpolated;
};




class Select : public Context
{
public:
	struct Props
	{
		SDL_FRect rect{ 0.f, 0.f, 0.f, 0.f };
		SDL_FRect inner_block_rect{ 0.f, 0.f, 0.f, 0.f };
		SDL_FRect text_margin{ 0.f, 0.f, 0.f, 0.f };
		SDL_Color text_color{ 0x00, 0x00, 0x00, 0xff };
		SDL_Color bg_color{ 0xff, 0xff, 0xff, 0xff };
		SDL_Color inner_block_bg_color{ 0xff, 0xff, 0xff, 0xff };
		SDL_Color on_hover_color{ 0x00,0x00,0x00,0x00 };
		float border_size = 1.f;
		float corner_radius = 1.f;
		int maxValues = 1;
		bool show_all_on_hover = false;
	};

public:
	struct Value
	{
		std::size_t id = 0;
		std::string value = "";
		std::string img = "";
		std::any user_data;
		std::vector<Value> inner_values;
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

    std::size_t size(){ return values_.size(); }

public:
	Select& Build(Context* _context, Select::Props _props, std::vector<Value> _values, std::size_t _default = 0)
	{
		Context::setContext(_context);
		Context::setView(viewValue.getView());
		// if any of _values contains img then add img padding to all value cells
		const auto bounds = _props.rect;
		/*_props.inner_block_rect.x = bounds.x;
		_props.inner_block_rect.y = bounds.y + bounds.h + 2.f;*/
		values_ = _values;
		defaultVal = _default;
		std::size_t _max_values = _values.size();
		_props.inner_block_rect.x = bounds.x + _props.inner_block_rect.x;
		_props.inner_block_rect.y = bounds.y + bounds.h + 1.f;

		// if all values rect dimensions arent set we defaut
		if (_props.inner_block_rect.w <= 0.f)
		{
			// float tmpH = _max_values >= 4 ? 4.f : static_cast<float>(_max_values);
			_props.inner_block_rect.w = bounds.w;
		}
		if (_props.inner_block_rect.h <= 0.f)
		{
			float tmpH = _values.size() > 0 ? (float)_values.size() : static_cast<float>(_max_values) * bounds.w;
			// tmpH = std::clamp();
			_props.inner_block_rect.h = bounds.h * tmpH;
		}
		props = _props;

		viewValue.setContext(_context);
		viewValue.bg_color = _props.bg_color;
		viewValue.corner_radius = _props.corner_radius;
		viewValue.bounds = bounds;
		if (_values.size())
		{
			viewValue.addTextBox(
				{//.mem_font = Font::OpenSansSemiBold,
				 .rect = {2.f, 5.f, 96.f, 90.f},
				 .textAttributes = {values_[_default].value, props.text_color, {0, 0, 0, 0}},
				 .gravity = Gravity::Left });
		}

		viewValue.registerCustomEventHandlerCallback([this](Cell& _cell) -> bool
			{
				viewValue.prevent_default_behaviour = true;
				if (valuesBlock.handleEvent())
				{
					return true;
				}
				if (event->type == SDL_KEYDOWN)
				{
					if (event->key.keysym.scancode == SDL_SCANCODE_AC_BACK && not valuesBlock.isHidden())
					{
						valuesBlock.hide();
						return true;
					}
				}
				if (event->type == EVT_FINGER_DOWN)
				{
					//mouse point
					auto mp = SDL_FPoint{ event->tfinger.x * DisplayInfo::Get().RenderW,
										 event->tfinger.y * DisplayInfo::Get().RenderH };
					//SDL_Log("d:%f,%f", mp.x, mp.y);
					//SDL_Log("s:%f,%f,%f,%f", _cell.bounds.x, _cell.bounds.y, _cell.bounds.w, _cell.bounds.h);
					if (_cell.isPointInBound(mp.x, mp.y) /* or p2*/)
					{
						viewValue.childViews[0]->toggleView();
						return true;
					}
					else
					{
						if (not valuesBlock.isHidden())
						{
							valuesBlock.hide();
							return true;
						}
						valuesBlock.hide();
					}
				}
				return false; });
		auto addBlock = [this, &bounds, on_hover_col = _props.on_hover_color](Cell& cell, CellBlock& cb) {
			cb.setCellRect(cell, 1, bounds.h, 0.f, 1.f);
			cell.bg_color = viewValue.bg_color;
			cell.onHoverBgColor = on_hover_col;
			cell.highlightOnHover = true;
			cell.addTextBox(
				{//.mem_font = Font::OpenSansSemiBold,
				 .rect = {2.f, 5.f, 96.f, 90.f},
				 .textAttributes = {values_[cell.index].value, props.text_color, {0, 0, 0, 0}},
				 .gravity = Gravity::Left });

			cell.textBox[0].onClick([this, &cell](TextBox* _textbox)
				{
					viewValue.textBox.pop_back();
					selectedVal = cell.index;
					viewValue.addTextBox(
						{//.mem_font = Font::OpenSansSemiBold,
						 .rect = {2.f, 5.f, 96.f, 90.f},
						 .textAttributes = {values_[selectedVal].value, props.text_color, {0, 0, 0, 0}},
						 .gravity = Gravity::Left });
					if (layers.contains(cell.index)) {
						layers[cell.index].show();
					}
					else {
						valuesBlock.hide();
					}

					if (onSelectCB != nullptr)
					{
						onSelectCB(values_[selectedVal]);
					}
					return true;
				});

			// check for inner layers
			if (not values_[cell.index].inner_values.empty()) {
				layers[cell.index] = CellBlock{};
				auto& inner_cb = layers[cell.index];

				inner_cb.setOnFillNewCellData(
					[_inner_values = values_[cell.index].inner_values, &inner_cb, this, bounds, on_hover_col = on_hover_col](Cell& _cell)
					{
						inner_cb.setCellRect(_cell, 1, bounds.h, 0.f, 1.f);
						_cell.bg_color = viewValue.bg_color;
						_cell.onHoverBgColor = on_hover_col;
						_cell.highlightOnHover = true;
						_cell.addTextBox(
							{//.mem_font = Font::OpenSansSemiBold,
							 .rect = {2.f, 5.f, 96.f, 90.f},
							 .textAttributes = {_inner_values[_cell.index].value, props.text_color, {0, 0, 0, 0}},
							 .gravity = Gravity::Left });
						// GLogger.Log(Logger::Level::Info, "trx:" + std::to_string(_cell.textBox[0].getRealPosX()) + "  try:" + std::to_string(_cell.textBox[0].getRealPosY()));
					});
				auto rect = props.inner_block_rect;
				rect.x -= rect.w;
				rect.x -= 2.f;
				inner_cb.Build(cell.getContext(), values_[cell.index].inner_values.size(), 1,
					{ .rect = rect,
					 .cornerRadius = props.corner_radius,
					 .bgColor = props.inner_block_bg_color });
				inner_cb.label = "inner cell block";
				cell.addChildView(inner_cb.getView()->show());
			}

			};

		valuesBlock.setOnFillNewCellData(
			[this, bounds, on_hover_col = _props.on_hover_color](Cell& _cell)
			{
				valuesBlock.setCellRect(_cell, 1, bounds.h, 0.f, 1.f);
				_cell.bg_color = viewValue.bg_color;
				_cell.onHoverBgColor = on_hover_col;
				_cell.highlightOnHover = true;
				_cell.addTextBox(
					{//.mem_font = Font::OpenSansSemiBold,
					 .rect = {2.f, 5.f, 96.f, 90.f},
					 .textAttributes = {values_[_cell.index].value, props.text_color, {0, 0, 0, 0}},
					 .gravity = Gravity::Left });
				// GLogger.Log(Logger::Level::Info, "trx:" + std::to_string(_cell.textBox[0].getRealPosX()) + "  try:" + std::to_string(_cell.textBox[0].getRealPosY()));
				_cell.textBox[0].onClick([this, &_cell](TextBox* _textbox)
					{
						viewValue.textBox.pop_back();
						selectedVal = _cell.index;
						viewValue.addTextBox(
							{//.mem_font = Font::OpenSansSemiBold,
							 .rect = {2.f, 5.f, 96.f, 90.f},
							 .textAttributes = {values_[selectedVal].value, props.text_color, {0, 0, 0, 0}},
							 .gravity = Gravity::Left });
						if (layers.contains(_cell.index)) {
							layers[_cell.index].show();
						}
						else {
							valuesBlock.hide();
						}

						if (onSelectCB != nullptr)
						{
							onSelectCB(values_[selectedVal]);
						}
						return true;
					});

				// check for inner layers
				if (not values_[_cell.index].inner_values.empty()) {
					layers[_cell.index] = CellBlock{};
					auto& cb = layers[_cell.index];

					cb.setOnFillNewCellData(
						[_inner_values= values_[_cell.index].inner_values, &cb,this, bounds, on_hover_col = on_hover_col](Cell& _cell)
						{
							cb.setCellRect(_cell, 1, bounds.h, 0.f, 1.f);
							_cell.bg_color = viewValue.bg_color;
							_cell.onHoverBgColor = on_hover_col;
							_cell.highlightOnHover = true;
							_cell.addTextBox(
								{//.mem_font = Font::OpenSansSemiBold,
								 .rect = {2.f, 5.f, 96.f, 90.f},
								 .textAttributes = {_inner_values[_cell.index].value, props.text_color, {0, 0, 0, 0}},
								 .gravity = Gravity::Left });
							// GLogger.Log(Logger::Level::Info, "trx:" + std::to_string(_cell.textBox[0].getRealPosX()) + "  try:" + std::to_string(_cell.textBox[0].getRealPosY()));
						});
					auto rect = props.inner_block_rect;
					rect.x -= rect.w;
					rect.x -= 2.f;
					cb.Build(_cell.getContext(), values_[_cell.index].inner_values.size(), 1,
						{ .rect = rect,
						 .cornerRadius = props.corner_radius,
						 .bgColor = props.inner_block_bg_color });
					cb.label = "inner cell block";
					_cell.addChildView(cb.getView()->show());
				}
			});
		valuesBlock.Build(_context, _max_values, 1,
			{ .rect = _props.inner_block_rect,
			 .cornerRadius = _props.corner_radius,
			 .bgColor = _props.inner_block_bg_color,
			.cellSpacingX=0.f,
			.cellSpacingY=0.f
			});
		valuesBlock.auto_hide = true;
		/*
		valuesBlock.getView()->rel_x=bounds.x;
		valuesBlock.getView()->rel_y=bounds.y;*/
		viewValue.addChildView(valuesBlock.getView()->hide());

		return *this;
	}

	Select&
		Build(Context* _context, Select::Props _props, int _max_values, const int& _numVerticalGrid, std::function<void(Cell&)> _valueCellOnCreateCallback)
	{
		Context::setContext(_context);
		Context::setView(viewValue.getView());
		auto bounds = _props.rect;
		// if all values rect dimensions arent set we defaut
		if (_props.inner_block_rect.w <= 0.f or _props.inner_block_rect.w)
		{
			float tmpH = _max_values >= 4 ? 4.f : static_cast<float>(_max_values);
			_props.inner_block_rect = { bounds.x, bounds.y + bounds.h, bounds.w, bounds.h * tmpH };
		}

		viewValue.setContext(_context);
		viewValue.bg_color = _props.bg_color;
		viewValue.corner_radius = _props.corner_radius;
		viewValue.bounds = bounds;

		valuesBlock.setOnFillNewCellData(_valueCellOnCreateCallback);
		valuesBlock.Build(_context, _max_values, _numVerticalGrid,
			{ .rect = _props.inner_block_rect,
			 .bgColor = _props.bg_color });

		return *this;
	}

	auto onSelect(std::function<void(Value&)> onselect)
	{
		onSelectCB = onselect;
	}

	// void addValueFront(Value newVal) { }

	void addValue(Value newVal)
	{
		if (values_.empty())
		{
			viewValue.addTextBox(
				{//.mem_font = Font::OpenSansSemiBold,
				 .rect = {2.f, 5.f, 96.f, 90.f},
				 .textAttributes = {newVal.value, {0, 0, 0, 0xff}, {0, 0, 0, 0}},
				 .gravity = Gravity::Left });
		}
		values_.emplace_back(newVal);
		valuesBlock.updateNoMaxCells(values_.size());
		valuesBlock.clearAndReset();
	}

	void removeValue(std::size_t val_index)
	{
		if (not values_.empty())
		{
			values_.erase(values_.begin() + val_index);
		}
		valuesBlock.updateNoMaxCells(values_.size());
		valuesBlock.clearAndReset();

		// update viewValue
		if (val_index == selectedVal and not values_.empty())
		{
			viewValue.textBox.pop_back();
			viewValue.addTextBox(
				{//.mem_font = Font::OpenSansSemiBold,
				 .rect = {2.f, 5.f, 96.f, 90.f},
				 .textAttributes = {values_[defaultVal].value, {0, 0, 0, 0xff}, {0, 0, 0, 0}},
				 .gravity = Gravity::Left });
		}
	}

	Value& getSelectedValue()
	{
		return values_[selectedVal];
	}

	Value& getValue(std::size_t index)
	{
		return values_[index];
	}

	std::size_t getSelectedValueIndex() const
	{
		return selectedVal;
	}

	std::vector<Value> getAllValues() { return values_; }

	bool empty() { return values_.empty(); }

	void reset() { values_ = {}; }

	bool handleEvent()
	{
		/*
		switch (event->type)
		{
		}*/
		return viewValue.handleEvent();
	}

	void draw()
	{
		viewValue.draw();
		for (auto& [a, b] : layers) {
			b.draw();
		}
	}

private:
	Cell viewValue{};
	CellBlock valuesBlock{};
	std::unordered_map<std::size_t, CellBlock> layers{};
	Props props;
	std::vector<Value> values_{};
	std::size_t selectedVal = 0;
	std::size_t defaultVal = 0;
	/// @todo implemment the show on over funtionality
	bool show_all_on_hover = false;
	std::function<void(Value&)> onSelectCB = nullptr;
};



using MenuProps = CellBlockProps;

class Menu : Context, public IView
{
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

	Menu &onClick(std::function<void(Cell &)> on_cell_clicked_callback)
	{
		menu_block.onClick(std::move(on_cell_clicked_callback));
		return *this;
	}

	bool handleEvent() override
	{
		return menu_block.handleEvent();
	}

	void draw() override
	{
		menu_block.draw();
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

void drawArcAntiAliased(SDL_Renderer *renderer, int centerX, int centerY, int radius, float startAngle, float endAngle, const SDL_Color &color)
{
	if (radius <= 0)
	{
		return; // Nothing to draw
	}

	const float angleStep = 0.5f; // Adjust for anti-aliasing quality vs. performance
	const float angleDiff = endAngle - startAngle;
	if (angleDiff == 0.0f)
		return;

	for (float angle = 0; angle <= angleDiff; angle += angleStep)
	{
		float rad = (startAngle + angle) * M_PI / 180.0f;
		float x = centerX + radius * cosf(rad);
		float y = centerY + radius * sinf(rad);

		// Simple anti-aliasing: blend with nearby pixels
		/*for (int dx = -1; dx <= 1; ++dx) {
			for (int dy = -1; dy <= 1; ++dy) {
				float dist = sqrtf(dx * dx + dy * dy);
				float weight = std::max(0.0f, 1.0f - dist / 1.5f); // Adjust for blending effect
				if (weight > 0) {
					SDL_Color blendedColor = {
						color.r,
						color.g,
						color.b,
						static_cast<Uint8>(color.a * weight),
					};
					SDL_SetRenderDrawColor(renderer, blendedColor.r, blendedColor.g, blendedColor.b, blendedColor.a);
					SDL_RenderDrawPointF(renderer, x + dx, y + dy);
				}
			}
		}*/
		SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
		SDL_RenderDrawPointF(renderer, x, y);
	}
}
