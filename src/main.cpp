#define NOMINMAX
#include <Windows.h>
#include <ViGEm/Client.h>
#include <iostream>
#include <print>
#include <bitset>
#include <chrono>	
#include <fstream>
#include <thread>
#include <json/json.hpp>
#include <interception/interception.h>


struct Binding
{
	USHORT buttons{ 0 };
	BYTE dpad{ DS4_BUTTON_DPAD_NONE };
	BYTE lx{ 128 };
	BYTE ly{ 128 };
	BYTE rt{};
	BYTE lt{};
};

float sensitivity = .9f;
float decay = 0.7f;
float curve = 1.5f;


auto parse_json(std::unordered_map<int, Binding>& um) -> void
{
	using json = nlohmann::json;
	std::ifstream ifs("bindings.json");
	const auto data = json::parse(ifs);


	if (data.contains("config"))
	{
		auto& c = data["config"];
		sensitivity = c.value("sensitivity", sensitivity);
		decay = c.value("decay", decay);
		curve = c.value("curve", curve);
	}

	for (const auto& b : data["bindings"])
	{
		if (b.contains("button"))
		{
			um.emplace(
				b["input"],
				Binding{
					.buttons{b["button"]}
				}
			);
		}
		else if (b.contains("stick"))
		{
			um.emplace(
				b["input"],
				Binding{
					.lx{b["stick"]["lx"]},
					.ly{b["stick"]["ly"]}
				}
			);
		}
		else if (b.contains("dpad"))
		{
			um.emplace(
				b["input"],
				Binding{
					.dpad = static_cast<BYTE>(b["dpad"])
				}
			);
		}
		else if (b.contains("trigger"))
		{
			if (b["trigger"].contains("lt"))
			{
				um.emplace(
					b["input"],
					Binding{
						.lt{b["trigger"]["lt"]}
					}
				);
			}
			if (b["trigger"].contains("rt"))
			{
				um.emplace(
					b["input"],
					Binding{
						.rt{b["trigger"]["rt"]}
					}
				);
			}
		}
	}
}


BOOL WINAPI WinMain(
	_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPSTR lpCmdLine,
	_In_ int nShowCmd
)
{
	std::unordered_map<int, Binding> um;
	InterceptionContext context;
	InterceptionStroke stroke;
	bool active{ true };
	bool toggle{};
	// this was shown in the example
	//SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
	try
	{
		parse_json(um);
	}
	catch (...)
	{
		std::exit(-1);
	}
	PVIGEM_CLIENT client = vigem_alloc();


	context = interception_create_context();
	interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_ALL);
	interception_set_filter(context, interception_is_keyboard, INTERCEPTION_FILTER_KEY_ALL);
	if (client == nullptr)
	{
		std::exit(-1);
	}

	if (!VIGEM_SUCCESS(vigem_connect(client)))
	{
		vigem_free(client);
		std::exit(-1);
	}

	PVIGEM_TARGET ds4 = vigem_target_ds4_alloc();
	if (!VIGEM_SUCCESS(vigem_target_add(client, ds4)))
	{
		vigem_target_free(ds4);
		vigem_disconnect(client);
		vigem_free(client);
		std::exit(-1);
	}
	DS4_REPORT report;
	DS4_REPORT_INIT(&report);

	std::bitset<256> previous_keys;
	std::bitset<256> keys;
	int mouse_dx{};
	int mouse_dy{};
	float aimX{};
	float aimY{};
	auto last_update{ std::chrono::steady_clock::now() };
	while (true)
	{
		for (InterceptionDevice d = 1; d <= INTERCEPTION_MAX_DEVICE; ++d)
		{
			if (interception_is_invalid(d))
				continue;
			if (interception_receive(context, d, &stroke, 1) > 0)
			{
				if (interception_is_keyboard(d))
				{
					const InterceptionKeyStroke* key = (InterceptionKeyStroke*)&stroke;
					// num lock and pause cause issues
					switch (key->state)
					{
					case 0:
						keys.set(key->code);
						break;
					case 1:
						keys.reset(key->code);
						break;
						// keys that begin with E0
					case 2:
						keys.set(key->code + 128);
						break;
					case 3:
						keys.reset(key->code + 128);
						break;
						// keys that have multiple codes will not be supported for now lmao...
					case 4:
						break;
					case 5:
						break;
					default:
						std::unreachable();
					}

					if (!active)
					{
						interception_send(context, d, &stroke, 1);
						continue;
					}
				}
				else if (interception_is_mouse(d))
				{
					const InterceptionMouseStroke* mouse = (InterceptionMouseStroke*)&stroke;
					if (mouse->x || mouse->y)
					{
						mouse_dy = mouse->y;
						mouse_dx = mouse->x;
					}
					if (mouse->state)
					{
						switch (mouse->state)
						{
						case 1:
							keys.set(1ull + 230);
							break;
						case 2:
							keys.reset(1ull + 230);
							break;
						case 4:
							keys.set(2ull + 230);
							break;
						case 8:
							keys.reset(2ull + 230);
							break;
						case 16:
							keys.set(3ull + 230);
							break;
						case 32:
							keys.reset(3ull + 230);
							break;
						case 64:
							keys.set(4ull + 230);
							break;
						case 128:
							keys.reset(4ull + 230);
							break;
						case 256:
							keys.set(5ull + 230);
							break;
						case 512:
							keys.reset(5ull + 230);
							break;
							// I can't test sideways scroll so I don't know how many codes there are
						default:
							break;
						}
					}

					if (!active)
					{
						interception_send(context, d, &stroke, 1);
						continue;
					}
				}
			}
		}


		if (keys[211])
			std::exit(-1);

		bool curr_toggle_state = keys[210];
		if (curr_toggle_state && !toggle)
			active = !active;
		toggle = curr_toggle_state;

		if (!active)
			continue;
		report.wButtons = 0;
		report.bTriggerL = 0;
		report.bTriggerR = 0;
		BYTE dpad = DS4_BUTTON_DPAD_NONE;

		int lx{}, ly{};
		int ls_active{};

		for (int i = 0; i < keys.size(); ++i)
		{
			if (!keys[i] || !um.contains(i)) continue;

			const Binding& b = um[i];

			report.wButtons |= b.buttons;

			if (b.dpad != DS4_BUTTON_DPAD_NONE)
				dpad = b.dpad;

			report.bTriggerL = std::max(report.bTriggerL, b.lt);
			report.bTriggerR = std::max(report.bTriggerR, b.rt);

			if (b.lx != 128 || b.ly != 128)
			{
				lx += b.lx - 128;
				ly += b.ly - 128;
				++ls_active;
			}
		}

		DS4_SET_DPAD(&report, static_cast<DS4_DPAD_DIRECTIONS>(dpad));

		BYTE final_lx{ 128 }, final_ly{ 128 };
		if (ls_active > 0)
		{
			final_lx = std::clamp<BYTE>(128 + lx, 0, 255);
			final_ly = std::clamp<BYTE>(128 + ly, 0, 255);
		}

		report.bThumbLX = final_lx;
		report.bThumbLY = final_ly;

		aimX += mouse_dx * sensitivity;
		aimY += mouse_dy * sensitivity * 1.5f;

		constexpr auto update_interval = std::chrono::milliseconds(17);
		const auto now = std::chrono::steady_clock::now();
		if (now - last_update >= update_interval)
		{
			if (mouse_dx == 0 && mouse_dy == 0)
			{
				aimX *= decay;
				aimY *= decay;
			}
			if (std::abs(aimX) < 0.3f) aimX = 0.0f;
			if (std::abs(aimY) < 0.3f) aimY = 0.0f;
			last_update = now;
		}

		auto apply_curve = [](float value, float factor) -> float {
			float sign = (value >= 0.0f) ? 1.0f : -1.0f;
			float normalized = std::min(std::abs(value) / 127.0f, 1.0f);
			float curved = std::pow(normalized, 1.0f / factor);
			return curved * 127.0f * sign;
		};

		const float stick_rx = std::clamp(128.f + aimX, 0.f, 255.f);
		const float stick_ry = std::clamp(128.f + aimY, 0.f, 255.f);

		const float curved_x = apply_curve(aimX, curve);
		const float curved_y = apply_curve(aimY, curve);

		report.bThumbRX = std::clamp(128 + static_cast<int>(curved_x), 0, 255);
		report.bThumbRY = std::clamp(128 + static_cast<int>(curved_y), 0, 255);

		mouse_dx = 0;
		mouse_dy = 0;
		vigem_target_ds4_update(client, ds4, report);
	}
	vigem_target_remove(client, ds4);
	vigem_target_free(ds4);
	vigem_disconnect(client);
	vigem_free(client);

	return 0;
}