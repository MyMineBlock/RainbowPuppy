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
	std::uint16_t buttons{};
	std::uint16_t dpad{ 8 };
	std::uint8_t lx{ 128 };
	std::uint8_t ly{ 128 };
	std::uint8_t rt{};
	std::uint8_t lt{};
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
					.dpad{b["dpad"]}
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

		int current_dpad{DS4_BUTTON_DPAD_NONE};
		int lx{}, ly{};
		int lx_active{};

		for (int i{}; i != keys.size(); ++i)
		{
			if (!keys[i] || !um.contains(i))
				continue;

			const auto& b = um[i];

			if (current_dpad == DS4_BUTTON_DPAD_NONE && b.dpad != 8)
			{
				current_dpad = b.dpad;
			}

			if (b.lx != 128 || b.ly != 128)
			{
				lx += static_cast<int>(b.lx) - 128;
				ly += static_cast<int>(b.ly) - 128;
				++lx_active;
			}
		}

		DS4_SET_DPAD(&report, static_cast<DS4_DPAD_DIRECTIONS>(current_dpad));

		if (lx_active > 0)
		{
			report.bThumbLX = std::clamp(128 + lx, 0, 255);
			report.bThumbLY = std::clamp(128 + ly, 0, 255);
		}
		else
		{
			report.bThumbLX = 128;
			report.bThumbLY = 128;
		}

		int finalLX{ 128 }, finalLY{ 128 };
		if (lx_active > 0)
		{
			finalLX = std::clamp(128 + lx, 0, 255);
			finalLY = std::clamp(128 + ly, 0, 255);
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

		report.bThumbRX = static_cast<std::uint8_t>(stickRX);
		report.bThumbRY = static_cast<std::uint8_t>(stickRY);

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


//void RegisterRawInput(HWND hwnd)
//{
//	RAWINPUTDEVICE rid;
//	rid.usUsagePage = 0x01; // Generic desktop controls
//	rid.usUsage = 0x02;     // Mouse
//	rid.dwFlags = RIDEV_INPUTSINK; // Receive input even if not in focus
//	rid.hwndTarget = hwnd;
//
//	if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) {
//	}
//}
//
//LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
//{
//	if (uMsg == WM_INPUT)
//	{
//		UINT dwSize = 0;
//		GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &dwSize, sizeof(RAWINPUTHEADER));
//		BYTE* lpb = new BYTE[dwSize];
//
//		if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, lpb, &dwSize, sizeof(RAWINPUTHEADER)) == dwSize)
//		{
//			RAWINPUT* raw = (RAWINPUT*)lpb;
//
//			if (raw->header.dwType == RIM_TYPEMOUSE)
//			{
//				// up is negative 
//				// left is negative
//				mouse_dx = raw->data.mouse.lLastX;
//				mouse_dy = raw->data.mouse.lLastY;
//			}
//		}
//
//		delete[] lpb;
//		return 0;
//	}
//
//	return DefWindowProc(hwnd, uMsg, wParam, lParam);
//}
//
//LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
//{
//	if (nCode == HC_ACTION)
//	{
//		DWORD key_code{ reinterpret_cast<LPKBDLLHOOKSTRUCT>(lParam)->vkCode };
//		bool isKeyDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
//		bool isKeyUp = (wParam == WM_KEYUP || wParam == WM_SYSKEYUP);
//
//		if (isKeyDown)
//		{
//			keys.set(key_code);
//		}
//		else if (isKeyUp)
//		{
//			keys.reset(key_code);
//		}
//	}
//
//	//return CallNextHookEx(0, nCode, wParam, lParam);
//	return 1;
//}
//
//LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam)
//{
//	if (nCode == HC_ACTION)
//	{
//		const auto mouse_data{ reinterpret_cast<LPMSLLHOOKSTRUCT>(lParam)->mouseData };
//		switch (wParam)
//		{
//		case WM_LBUTTONDOWN:
//			keys.set(257);
//			break;
//		case WM_RBUTTONDOWN:
//			keys.set(258);
//			break;
//		case WM_MBUTTONDOWN:
//			keys.set(259);
//			break;
//			//todo WM_MOUSEWHEEL it is not a toglle typa thing... therefore not a real keypress
//		case WM_XBUTTONDOWN:
//		{
//			if (HIWORD(mouse_data) == XBUTTON1)
//			{
//				keys.set(260);
//				std::exit(0);
//			}
//			else if (HIWORD(mouse_data) == XBUTTON2)
//				keys.set(261);
//			break;
//		}
//		case WM_LBUTTONUP:
//			keys.reset(257);
//			break;
//		case WM_RBUTTONUP:
//			keys.reset(258);
//			break;
//		case WM_MBUTTONUP:
//			keys.reset(259);
//			break;
//		case WM_XBUTTONUP:
//		{
//			if (HIWORD(mouse_data) == XBUTTON1)
//				keys.reset(260);
//			else if (HIWORD(mouse_data) == XBUTTON2)
//				keys.reset(261);
//			break;
//		}
//		default:
//			break;
//		}
//	}
//	//return CallNextHookEx(0, nCode, wParam, lParam);
//	return 1;
//}
//
// 
// 
//BOOL WINAPI WinMain(
//	_In_ HINSTANCE hInstance,
//	_In_opt_ HINSTANCE hPrevInstance,
//	_In_ LPSTR lpCmdLine,
//	_In_ int nShowCmd
//)
//int main() 
//{
//	std::unordered_map<int, Binding> um;
//	InterceptionContext context;
//	InterceptionStroke stroke;
//	std::jthread t(nuke);
//	//SetPriorityClass(GetCurrentProcess(), HIGH_PRIORITY_CLASS);
//	try
//	{
//		parse_json(um);
//	}
//	catch (...)
//	{
//		std::exit(-1);
//	}
//	PVIGEM_CLIENT client = vigem_alloc();
//
//	context = interception_create_context();
//	interception_set_filter(context, interception_is_mouse, INTERCEPTION_FILTER_MOUSE_ALL);
//	interception_set_filter(context, interception_is_keyboard, INTERCEPTION_FILTER_KEY_ALL);
//	if (client == nullptr) 
//	{
//		MessageBox(nullptr, L"Big big issues", L"Dadda", MB_OK);
//		std::exit(-1);
//	}
//
//	if (!VIGEM_SUCCESS(vigem_connect(client))) 
//	{
//		vigem_free(client);
//		MessageBox(nullptr, L"Big big issues", L"Dadda", MB_OK);
//		std::exit(-1);
//	}
//
//	PVIGEM_TARGET ds4 = vigem_target_ds4_alloc();
//	if (!VIGEM_SUCCESS(vigem_target_add(client, ds4))) 
//	{
//		vigem_target_free(ds4);
//		vigem_disconnect(client);
//		vigem_free(client);
//		MessageBox(nullptr, L"Big big issues", L"Dadda", MB_OK);
//		std::exit(-1);
//	}
//	/*WNDCLASS wc = {};
//	wc.lpfnWndProc = WindowProc;
//	wc.hInstance = GetModuleHandle(nullptr);
//	wc.lpszClassName = L"RawMouseWindow";*/
//
//	/*RegisterClass(&wc);
//	HWND hwnd = CreateWindowEx(0, wc.lpszClassName, L"Raw Input", 0,
//		0, 0, 100, 100, nullptr, nullptr, GetModuleHandle(nullptr), nullptr);
//	RegisterRawInput(hwnd);
//	ShowWindow(hwnd, SW_HIDE);*/
//
//	DS4_REPORT report{};
//	report.bThumbLX = 0x80; 
//	report.bThumbLY = 0x80;
//	report.bThumbRX = 0x80;
//	report.bThumbRY = 0x80;
//	DS4_SET_DPAD(&report, DS4_BUTTON_DPAD_NONE);
//
//	//auto keyhook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, nullptr, 0);
//	//auto mousehook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, nullptr, 0);
//	//MSG msg{};
//	
//
//	std::bitset<524> previous_keys;
//	std::bitset<524> keys;
//	int mouse_dx{};
//	int mouse_dy{};
//
//	float aimX{};
//	float aimY{};
//	auto last_update = std::chrono::steady_clock::now();
//	while (true) 
//	{
//		for (InterceptionDevice d = 1; d <= INTERCEPTION_MAX_DEVICE; ++d)
//		{
//			if (interception_is_invalid(d)) 
//				continue;
//			if (interception_receive(context, d, &stroke, 1) > 0)
//			{
//				if (interception_is_keyboard(d))
//				{
//					const InterceptionKeyStroke* key = (InterceptionKeyStroke*)&stroke;
//					//std::println("Key: {}, state: {}", key->code, key->state);
//
//					// num lock and pause cause issues
//					switch (key->state)
//					{
//					case 0:
//						keys.set(key->code);
//						break;
//					case 1:
//						keys.reset(key->code);
//						break;
//						// keys that begin with E0
//					case 2:
//						keys.set(key->code + 128);
//						break;
//					case 3:
//						keys.set(key->code + 128);
//						break;
//						// keys that have multiple codes will not be supported for now lmao...
//					case 4:
//						break;
//					case 5:
//						break;
//					default:
//						std::unreachable();
//					}
//					//interception_send(context, d, &stroke, 1);
//				}
//				else if (interception_is_mouse(d))
//				{
//					const InterceptionMouseStroke* mouse = (InterceptionMouseStroke*)&stroke;
//					std::println("x: {}, y: {}, rolling: {}, state: {}", mouse->x, mouse->y, mouse->rolling, mouse->state);
//					if (mouse->x || mouse->y)
//					{
//						mouse_dy = mouse->y;
//						mouse_dx = mouse->x;
//					}
//					if (mouse->state)
//					{
//						switch (mouse->state)
//						{
//						case 1:
//							keys.set(1ull + 512);
//							break;
//						case 2:
//							keys.reset(1ull + 512);
//							break;
//						case 4:
//							keys.set(2ull + 512);
//							break;
//						case 8:
//							keys.reset(2ull + 512);
//							break;
//						case 16:
//							keys.set(3ull + 512);
//							break;
//						case 32:
//							keys.reset(3ull + 512);
//							break;
//						case 64:
//							keys.set(4ull + 512);
//							break;
//						case 128:
//							keys.reset(4ull + 512);
//							break;
//						case 256:
//							keys.set(5ull + 512);
//							break;
//						case 512:
//							keys.reset(5ull + 512);
//							break;
//						}
//					}
//					/* // I may decide to add scrolling or something but meh
//					if (mouse->rolling)
//					{
//
//					}
//					*/
//					
//					//interception_send(context, d, &stroke, 1);
//				}
//			}
//		}
//
//		if (keys[211])
//		{
//			std::exit(-1);
//		}
//		
//
//		/*if (PeekMessage(&msg, NULL, NULL, NULL, PM_REMOVE))
//		{
//			TranslateMessage(&msg);
//			DispatchMessage(&msg);
//		}*/
//		
//
//		for (int i{}; i < keys.size(); ++i)
//		{
//			if (keys[i] && !previous_keys[i])
//			{
//				previous_keys.set(i);
//				if (um.contains(i))
//				{
//					report.wButtons |= um[i].buttons;
//				}
//			}
//			if (!keys[i] && previous_keys[i])
//			{
//				previous_keys.reset(i);
//				if (um.contains(i))
//				{
//					report.wButtons &= ~um[i].buttons;
//				}
//			}
//		}
//		
//		int current_dpad = DS4_BUTTON_DPAD_NONE;
//
//		for (int i{}; i < keys.size(); ++i)
//		{
//			if (!keys[i]) 
//				continue;
//			if (!um.contains(i))
//				continue;
//
//			const auto& b = um[i];
//			if (b.dpad != 8)
//			{
//				current_dpad = b.dpad;
//				break;
//			}
//		}
//		DS4_SET_DPAD(&report, (DS4_DPAD_DIRECTIONS)current_dpad);
//		
//		int lx = 0, ly = 0, rx = 0, ry = 0;
//		int lx_active = 0, rx_active = 0;
//
//		for (int i{}; i < keys.size(); ++i)
//		{
//			if (!keys[i]) 
//				continue;
//			if (!um.contains(i))
//				continue;
//
//			const auto& b = um[i];
//
//			if (b.lx != 128 || b.ly != 128)
//			{
//				lx += (int)b.lx - 128;
//				ly += (int)b.ly - 128;
//				++lx_active;
//			}
//		}
//
//		int finalLX = 128, finalLY = 128;
//		if (lx_active > 0)
//		{
//			finalLX = std::clamp(128 + lx, 0, 255);
//			finalLY = std::clamp(128 + ly, 0, 255);
//		}
//
//		report.bThumbLX = finalLX;
//		report.bThumbLY = finalLY;
//
//
//		aimX += mouse_dx * sensitivity;
//		aimY += mouse_dy * sensitivity;
//		
//
//		constexpr auto update_interval = std::chrono::milliseconds(17);
//		auto now = std::chrono::steady_clock::now();
//		if (now - last_update >= update_interval) 
//		{
//			if (mouse_dx == 0 && mouse_dy == 0) 
//			{
//				aimX *= decay;
//				aimY *= decay;
//			}
//			if (std::abs(aimX) < 0.3f) aimX = 0.0f;
//			if (std::abs(aimY) < 0.3f) aimY = 0.0f;
//			last_update = now;
//		}
//
//
//		float stickRX = std::clamp(128.f + aimX, 0.f, 255.f);
//		float stickRY = std::clamp(128.f + aimY, 0.f, 255.f);
//
//		report.bThumbRX = static_cast<std::uint8_t>(stickRX);
//		report.bThumbRY = static_cast<std::uint8_t>(stickRY);
//
//		mouse_dx = 0;
//		mouse_dy = 0;
//		vigem_target_ds4_update(client, ds4, report);
//
//	}
//
//	//UnhookWindowsHookEx(keyhook);
//	//UnhookWindowsHookEx(mousehook);
//	vigem_target_remove(client, ds4);
//	vigem_target_free(ds4);
//	vigem_disconnect(client);
//	vigem_free(client);
//
//	return 0;
//}



//#include <windows.h>
//#include <setupapi.h>
//#include <initguid.h>
//#include <usbiodef.h>
//#include <winerror.h>
//#include <hidclass.h>
//#include <hidsdi.h>
//#include <iostream>
//#include <iomanip>
//#include <vector>
//#include <thread>
//#include <utility>
//#include <cstddef>
//
//#pragma comment(lib, "setupapi.lib")
//#pragma comment(lib, "hid.lib")
//
//void hueToRGB(float hue, BYTE& r, BYTE& g, BYTE& b) {
//    float s = 1.0f, v = 1.0f; // full saturation and brightness
//    float c = v * s;
//    float x = c * (1 - fabsf(fmodf(hue / 60.0f, 2) - 1));
//    float m = v - c;
//
//    float rf = 0, gf = 0, bf = 0;
//
//    if (hue < 60) { rf = c; gf = x; bf = 0; }
//    else if (hue < 120) { rf = x; gf = c; bf = 0; }
//    else if (hue < 180) { rf = 0; gf = c; bf = x; }
//    else if (hue < 240) { rf = 0; gf = x; bf = c; }
//    else if (hue < 300) { rf = x; gf = 0; bf = c; }
//    else { rf = c; gf = 0; bf = x; }
//
//    r = static_cast<BYTE>((rf + m) * 255);
//    g = static_cast<BYTE>((gf + m) * 255);
//    b = static_cast<BYTE>((bf + m) * 255);
//}
//
//void rainbowLightbar(HANDLE hDevice) {
//    std::vector<BYTE> outputReport(64, 0);
//    outputReport[0] = 0x02;  // HID report ID
//    outputReport[1] = 0xFF;  // enable flags
//    outputReport[2] = 0x04;  // enable lightbar
//
//    float hue = 0.0f;
//
//    while (true) {
//        BYTE r, g, b;
//        hueToRGB(hue, r, g, b);
//        outputReport[45] = r;
//        outputReport[46] = g;
//        outputReport[47] = b;
//
//        DWORD bytesWritten;
//        WriteFile(hDevice, outputReport.data(), static_cast<DWORD>(outputReport.size()), &bytesWritten, nullptr);
//
//        hue += 2.0f;
//        if (hue >= 360.0f) hue -= 360.0f;
//
//        std::this_thread::sleep_for(std::chrono::milliseconds(30));
//    }
//}
//
//
//int main() 
//{
//    GUID hidGuid;
//    HidD_GetHidGuid(&hidGuid);
//
//    HDEVINFO hHidInfo = SetupDiGetClassDevsW(&hidGuid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
//    if (hHidInfo == INVALID_HANDLE_VALUE) 
//    {
//        std::wcerr << L"SetupDiGetClassDevsW failed: " << GetLastError() << std::endl;
//        return 1;
//    }
//
//    SP_DEVICE_INTERFACE_DATA hidInterfaceData{};
//    hidInterfaceData.cbSize = sizeof(hidInterfaceData);
//    DWORD hidIndex{};
//    HANDLE hDevice = INVALID_HANDLE_VALUE;
//    PHIDP_PREPARSED_DATA ppd{};
//    HIDP_CAPS caps{};
//
//    while (SetupDiEnumDeviceInterfaces(hHidInfo, nullptr, &hidGuid, hidIndex++, &hidInterfaceData)) 
//    {
//        DWORD requiredSize{};
//        SetupDiGetDeviceInterfaceDetailW(hHidInfo, &hidInterfaceData, nullptr, 0, &requiredSize, nullptr);
//        std::vector<BYTE> detailBuffer(requiredSize);
//        auto detail = reinterpret_cast<PSP_DEVICE_INTERFACE_DETAIL_DATA_W>(detailBuffer.data());
//        detail->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
//        SP_DEVINFO_DATA devInfo{};
//        devInfo.cbSize = sizeof(devInfo);
//
//        if (!SetupDiGetDeviceInterfaceDetailW(hHidInfo, &hidInterfaceData, detail, requiredSize, nullptr, &devInfo))
//            continue;
//
//        std::wstring path = detail->DevicePath;
//        if (path.find(L"vid_054c&pid_0ce6") == std::wstring::npos)
//            continue;
//
//        std::wcout << L"Opening DualSense HID: " << path << std::endl;
//
//        hDevice = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
//            FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
//            OPEN_EXISTING, 0, nullptr);
//        if (hDevice == INVALID_HANDLE_VALUE) {
//            std::wcerr << L"Failed to open HID device: " << GetLastError() << std::endl;
//            continue;
//        }
//
//        if (!HidD_GetPreparsedData(hDevice, &ppd) ||
//            HidP_GetCaps(ppd, &caps) != HIDP_STATUS_SUCCESS)
//        {
//            std::wcerr << L"HID preparsed data or caps retrieval failed" << std::endl;
//            CloseHandle(hDevice);
//            hDevice = INVALID_HANDLE_VALUE;
//            continue;
//        }
//        
//        break;
//    }
//    SetupDiDestroyDeviceInfoList(hHidInfo);
//
//    if (hDevice == INVALID_HANDLE_VALUE) {
//        std::wcerr << L"Could not find or open DualSense HID interface." << std::endl;
//        return 1;
//    }
//
//    //02 00 14 00 00 00 00 00 00 00
//    //00 00 00 00 00 00 00 00 00 00
//    //00 00 00 00 00 00 00 00 00 00
//    //00 00 00 00 00 00 00 00 00 00
//    //00 00 00 00 24 00 00 40
//
//    //02 00 14 00 00 00 00 00 00 00 
//    //00 00 00 00 00 00 00 00 00 00 
//    //00 00 00 00 00 00 00 00 00 00 
//    //00 00 00 00 24 00 00 40 00 00
//    //00 00 00 00 00 00 00 00
//
//    //outputReport[3] = 0x00;      // light vibration
//    //outputReport[4] = 0x00;      // hard vibration
//    //outputReport[5] = 0xFF;      // hard vibration
//    //outputReport[6] = 0xFF;      // hard vibration
//    //outputReport[7] = 0xFF;      // hard vibration
//    //outputReport[45] = 0xFF;     // Red
//    //outputReport[46] = 0x00;     // Green
//    //outputReport[47] = 0x00;     // Blue
//    //outputReport[48] = 0xFF; // Lightbar brightness (0x00 = off, 0xFF = full)
//    //outputReport[49] = 0x01;
//    //outputReport[50] = 0x01; 
//    //outputReport[53] = 0x04; 
//    //outputReport[54] = 0x01; 
//    DWORD bytesWritten;
//
//    // priming packet
//    std::vector<BYTE> outputReport = {
//    0x02, 0x00, 0x14,                       
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,      
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 
//    0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00                                   
//    };
//    WriteFile(hDevice, outputReport.data(), static_cast<DWORD>(outputReport.size()), &bytesWritten, nullptr);
//
//    // actual data packet
//    outputReport = {
//    0x02, 0x00, 0x14,                        
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00,
//    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
//    0x24, 0x00, 0x00,                        
//    0x40                                     
//    };
//    WriteFile(hDevice, outputReport.data(), static_cast<DWORD>(outputReport.size()), &bytesWritten, nullptr);
//
//    DWORD reportSize = caps.InputReportByteLength;
//    std::vector<BYTE> reportBuffer(reportSize);
//
//    std::wcout << L"Press Ctrl+C to exit and see button mapping info." << std::endl;
//    while (true) 
//    {
//        DWORD bytesRead = 0;
//        if (ReadFile(hDevice, reportBuffer.data(), reportSize, &bytesRead, nullptr)) 
//        {
//            // Print raw report
//            std::wcout << L"[" << GetTickCount64() << L"] Report: ";
//            for (DWORD i = 0; i < bytesRead; ++i) 
//            {
//                std::wcout << std::hex << std::uppercase << std::setw(2)
//                    << std::setfill(L'0') << (unsigned int)reportBuffer[i] << L" ";
//            }
//            std::wcout << std::dec << std::endl;
//
//            enum DPAD
//            {
//                UP,
//                UP_RIGHT,
//                RIGHT,
//                RIGHT_DOWN,
//                DOWN,
//                DOWN_LEFT,
//                LEFT,
//                LEFT_UP,
//                NONE
//            };
//            enum MainButtons
//            {
//                SQUARE = 1,
//                CROSS = 2,
//                CIRCLE = 4,
//                TRIANGLE = 8
//            };
//            if (bytesRead >= 22) 
//            {
//                BYTE dpad = reportBuffer[8] & 15;
//                BYTE buttons = reportBuffer[8] >> 4;
//                std::wcout << L"Buttons pressed: ";
//                std::wstring btn{};
//                switch (dpad)
//                {
//                case UP:
//                    btn = L"UP";
//                    break;
//                case UP_RIGHT:
//                    btn = L"UP_RIGHT";
//                    break;
//                case RIGHT:
//                    btn = L"RIGHT";
//                    break;
//                case RIGHT_DOWN:
//                    btn = L"RIGHT_DOWN";
//                    break;
//                case DOWN:
//                    btn = L"DOWN";
//                    break;
//                case DOWN_LEFT:
//                    btn = L"DOWN_LEFT";
//                    break;
//                case LEFT:
//                    btn = L"LEFT";
//                    break;
//                case LEFT_UP:
//                    btn = L"LEFT_UP";
//                    break;
//                case NONE:
//                    btn = L"NONE";
//                    break;
//                }
//                if (buttons & 1)
//                    std::wcout << "Square, ";
//                if(buttons & 2)
//                    std::wcout << "Cross, ";
//                if(buttons & 4)
//                    std::wcout << "Circle, ";
//                if(buttons & 8)
//                    std::wcout << "Triangle, ";
//                std::wcout << btn << std::endl;
//            }
//        }
//        else
//        {
//            std::wcerr << L"ReadFile failed: " << GetLastError() << std::endl;
//        }
//        std::this_thread::sleep_for(std::chrono::seconds(1));
//    }
//
//    HidD_FreePreparsedData(ppd);
//    CloseHandle(hDevice);
//    return 0;
//}
