//
// DirectXPage.xaml.cpp
// DirectXPage 类的实现。
//

#include "pch.h"
#include "DirectXPage.xaml.h"
#include <windows.ui.xaml.media.dxinterop.h>
#include "../libwtfdanmaku/include/WTFDanmaku.h"

using namespace WTF_UWP10_Demo;

using namespace Platform;
using namespace Windows::Foundation;
using namespace Windows::Foundation::Collections;
using namespace Windows::Graphics::Display;
using namespace Windows::System::Threading;
using namespace Windows::UI::Core;
using namespace Windows::UI::Input;
using namespace Windows::UI::Xaml;
using namespace Windows::UI::Xaml::Controls;
using namespace Windows::UI::Xaml::Controls::Primitives;
using namespace Windows::UI::Xaml::Data;
using namespace Windows::UI::Xaml::Input;
using namespace Windows::UI::Xaml::Media;
using namespace Windows::UI::Xaml::Navigation;
using namespace concurrency;
using namespace Microsoft::WRL;

DirectXPage::DirectXPage():
	m_windowVisible(true),
	m_coreInput(nullptr),
    wtf(nullptr)
{
	InitializeComponent();

	// 注册页面生命周期的事件处理程序。
	CoreWindow^ window = Window::Current->CoreWindow;

	window->VisibilityChanged +=
		ref new TypedEventHandler<CoreWindow^, VisibilityChangedEventArgs^>(this, &DirectXPage::OnVisibilityChanged);

	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	currentDisplayInformation->DpiChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &DirectXPage::OnDpiChanged);

	currentDisplayInformation->OrientationChanged +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &DirectXPage::OnOrientationChanged);

	DisplayInformation::DisplayContentsInvalidated +=
		ref new TypedEventHandler<DisplayInformation^, Object^>(this, &DirectXPage::OnDisplayContentsInvalidated);

	swapChainPanel->CompositionScaleChanged += 
		ref new TypedEventHandler<SwapChainPanel^, Object^>(this, &DirectXPage::OnCompositionScaleChanged);

	swapChainPanel->SizeChanged +=
		ref new SizeChangedEventHandler(this, &DirectXPage::OnSwapChainPanelSizeChanged);

	// 此时，我们具有访问设备的权限。
	// 我们可创建与设备相关的资源。
    wtf = WTF_CreateInstance();

	// 注册我们的 SwapChainPanel 以获取独立的输入指针事件
	auto workItemHandler = ref new WorkItemHandler([this] (IAsyncAction ^)
	{
		// 对于指定的设备类型，无论它是在哪个线程上创建的，CoreIndependentInputSource 都将引发指针事件。
		m_coreInput = swapChainPanel->CreateCoreIndependentInputSource(
			Windows::UI::Core::CoreInputDeviceTypes::Mouse |
			Windows::UI::Core::CoreInputDeviceTypes::Touch |
			Windows::UI::Core::CoreInputDeviceTypes::Pen
			);

		// 指针事件的寄存器，将在后台线程上引发。
		m_coreInput->PointerPressed += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &DirectXPage::OnPointerPressed);
		m_coreInput->PointerMoved += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &DirectXPage::OnPointerMoved);
		m_coreInput->PointerReleased += ref new TypedEventHandler<Object^, PointerEventArgs^>(this, &DirectXPage::OnPointerReleased);

		// 一旦发送输入消息，即开始处理它们。
		m_coreInput->Dispatcher->ProcessEvents(CoreProcessEventsOption::ProcessUntilQuit);
	});

	// 在高优先级的专用后台线程上运行任务。
	m_inputLoopWorker = ThreadPool::RunAsync(workItemHandler, WorkItemPriority::High, WorkItemOptions::TimeSliced);

    WTF_InitializeOffscreen(wtf, 960, 540);

    ComPtr<IDXGISwapChain> swapChain;
    WTF_QuerySwapChain(wtf, &__uuidof(IDXGISwapChain), &swapChain);
    ComPtr<ISwapChainPanelNative> panelNative;
    reinterpret_cast<IUnknown*>(swapChainPanel)->QueryInterface(IID_PPV_ARGS(&panelNative));
    panelNative->SetSwapChain(swapChain.Get());

    WTF_SetFontName(wtf, L"SimHei");
    WTF_SetFontWeight(wtf, 700);
    WTF_SetFontScaleFactor(wtf, 1.0f);
    WTF_SetCompositionOpacity(wtf, 0.95f);
    WTF_SetDanmakuTypeVisibility(wtf, WTF_DANMAKU_TYPE_SCROLLING_VISIBLE |
        WTF_DANMAKU_TYPE_TOP_VISIBLE |
        WTF_DANMAKU_TYPE_BOTTOM_VISIBLE);

    std::wstring path = Windows::ApplicationModel::Package::Current->InstalledLocation->Path->Data();
    path += L"\\Assets\\529622.xml";

    WTF_LoadBilibiliFileW(wtf, path.c_str());

    WTF_Start(wtf);
}

DirectXPage::~DirectXPage()
{
	// 析构时停止渲染和处理事件。
    if (wtf) {
        if (WTF_IsRunning(wtf)) {
            WTF_Stop(wtf);
        }
        WTF_Terminate(wtf);
        WTF_ReleaseInstance(wtf);
        wtf = nullptr;
    }
    m_coreInput->Dispatcher->StopProcessEvents();
}

// 针对挂起和终止事件保存应用程序的当前状态。
void DirectXPage::SaveInternalState(IPropertySet^ state)
{
    WTF_Stop(wtf);
}

// 针对恢复事件加载应用程序的当前状态。
void DirectXPage::LoadInternalState(IPropertySet^ state)
{
	// 在此处放置代码以加载应用程序状态。

	// 恢复应用程序时开始渲染。
    WTF_Start(wtf);
}

// 窗口事件处理程序。

void DirectXPage::OnVisibilityChanged(CoreWindow^ sender, VisibilityChangedEventArgs^ args)
{
    m_windowVisible = args->Visible;
    if (m_windowVisible)
    {
        WTF_Resume(wtf);
    }
    else
    {
        WTF_Pause(wtf);
    }
}

// DisplayInformation 事件处理程序。

void DirectXPage::OnDpiChanged(DisplayInformation^ sender, Object^ args)
{
    float dpiX = sender->RawDpiX;
    float dpiY = sender->RawDpiY;
    if (wtf) {
        WTF_SetDpi(wtf, static_cast<uint32_t>(dpiX), static_cast<uint32_t>(dpiY));
    }
}

void DirectXPage::OnOrientationChanged(DisplayInformation^ sender, Object^ args)
{

}


void DirectXPage::OnDisplayContentsInvalidated(DisplayInformation^ sender, Object^ args)
{

}

// 在单击应用程序栏按钮时调用。
void DirectXPage::AppBarButton_Click(Object^ sender, RoutedEventArgs^ e)
{
	// 使用应用程序栏(如果它适合你的应用程序)。设计应用程序栏，
	// 然后填充事件处理程序(与此类似)。
}

void DirectXPage::OnPointerPressed(Object^ sender, PointerEventArgs^ e)
{

}

void DirectXPage::OnPointerMoved(Object^ sender, PointerEventArgs^ e)
{

}

void DirectXPage::OnPointerReleased(Object^ sender, PointerEventArgs^ e)
{

}

void DirectXPage::OnCompositionScaleChanged(SwapChainPanel^ sender, Object^ args)
{
    float scaleX = sender->CompositionScaleX;
    float scaleY = sender->CompositionScaleY;
    WTF_Resize(wtf, static_cast<uint32_t>(sender->ActualWidth * scaleX), static_cast<uint32_t>(sender->ActualHeight * scaleY));
}

void DirectXPage::OnSwapChainPanelSizeChanged(Object^ sender, SizeChangedEventArgs^ e)
{
    WTF_Resize(wtf, static_cast<uint32_t>(e->NewSize.Width), static_cast<uint32_t>(e->NewSize.Height));
}
