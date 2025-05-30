//Copyright(c) 2025 MyMineBlock
//
//Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files(the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and /or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

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
	USHORT buttons{0x80};
	BYTE lx{ 128 };
	BYTE ly{ 128 };
	BYTE rt{};
	BYTE lt{};
};

constexpr float sensitivity = .9f;
constexpr float decay = 0.7f;


void parse_json(std::unordered_map<int, Binding>& um)
{
	using json = nlohmann::json;
	std::ifstream ifs("bindings.json");
	const auto data = json::parse(ifs);

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
					.buttons{static_cast<USHORT>(b["dpad"] << 8)}
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


int main()
{
	std::unordered_map<int, Binding> um;
	InterceptionContext context;
	InterceptionStroke stroke;
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
	//interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_ALL);
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
						keys.set(key->code + 128);
						break;
						// keys that have multiple codes will not be supported for now lmao...
					case 4:
						break;
					case 5:
						break;
					default:
						std::unreachable();
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
				}
			}
		}

		BYTE current_dpad{DS4_BUTTON_DPAD_NONE};
		BYTE lx{}, ly{};
		BYTE lx_active{};

		for (int i{}; i != keys.size(); ++i)
		{
			if (!keys[i] || !um.contains(i))
				continue;

			const auto& b = um[i];

			if (current_dpad == DS4_BUTTON_DPAD_NONE && b.buttons >> 8 != 8)
			{
				current_dpad = b.buttons >> 8;
			}

			if (b.lx != 128 || b.ly != 128)
			{
				lx += b.lx - 128;
				ly += b.ly - 128;
				++lx_active;
			}
		}

		DS4_SET_DPAD(&report, static_cast<DS4_DPAD_DIRECTIONS>(current_dpad));

		if (lx_active > 0)
		{
			report.bThumbLX = std::clamp<BYTE>(128 + lx, 0, 255);
			report.bThumbLY = std::clamp<BYTE>(128 + ly, 0, 255);
		}
		else
		{
			report.bThumbLX = 128;
			report.bThumbLY = 128;
		}

		BYTE finalLX{ 128 }, finalLY{ 128 };
		if (lx_active > 0)
		{
			finalLX = std::clamp<BYTE>(128 + lx, 0, 255);
			finalLY = std::clamp<BYTE>(128 + ly, 0, 255);
		}

		report.bThumbLX = finalLX;
		report.bThumbLY = finalLY;

		aimX += mouse_dx * sensitivity;
		aimY += mouse_dy * sensitivity;

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

		const float stickRX = std::clamp(128.f + aimX, 0.f, 255.f);
		const float stickRY = std::clamp(128.f + aimY, 0.f, 255.f);

		report.bThumbRX = static_cast<BYTE>(stickRX);
		report.bThumbRY = static_cast<BYTE>(stickRY);

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