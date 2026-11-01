// Window surface: create, compose, resize, minimize and destroy.
//
// The backend owns the window and the composed images. Each submitted frame is
// cleared with the requested colour, copied to a readback buffer before it is
// shown, and only then handed to the display. A minimized surface refuses new
// work with NotReady instead of recording into a zero-sized target. Resizing
// waits for all recorded work, recreates the buffers and advances the history
// generation.

#include "d3d12/d3d12_impl.h"

namespace doom3 {
namespace rhi {

using d3d12::detail::ChangeUse;
using d3d12::detail::Fail;

namespace {

std::atomic<uint32_t> gWindowClassSerial{1};

LRESULT CALLBACK SurfaceWindowProc(HWND window, UINT message, WPARAM wParam,
                                   LPARAM lParam) {
    if (message == WM_CLOSE) {
        // The test owns the lifetime; a stray close request must not invalidate
        // the surface between submissions.
        return 0;
    }
    if (message == WM_PAINT) {
        PAINTSTRUCT paint{};
        BeginPaint(window, &paint);
        EndPaint(window, &paint);
        return 0;
    }
    return DefWindowProcW(window, message, wParam, lParam);
}

std::wstring Widen(const std::string& value) {
    if (value.empty()) {
        return std::wstring();
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, value.c_str(),
                                           static_cast<int>(value.size()), nullptr, 0);
    if (length <= 0) {
        return std::wstring();
    }
    std::wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, value.c_str(), static_cast<int>(value.size()),
                        &result[0], length);
    return result;
}

void PumpMessages() {
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

}  // namespace

bool d3d12::detail::Q1bState::DrainQueue(DWORD timeoutMilliseconds, std::string* error) {
    if (!host->Queue() || !host->Completion()) {
        Fail(Status::Error, error, "device is not initialized");
        return false;
    }
    const uint64_t serial = host->NextSerial();
    if (serial == 0) {
        Fail(Status::Error, error, "completion value space exhausted");
        return false;
    }
    // A released submission no longer contributes to LastSerial(), so a fresh
    // queue signal is the only way to prove that every previously submitted
    // operation, including the last Present's queue work, has completed.
    const HRESULT signalled = host->Queue()->Signal(host->Completion(), serial);
    if (FAILED(signalled)) {
        Fail(Status::Error, error,
             "CommandQueue::Signal(surface drain) " + d3d12::detail::HrString(signalled));
        return false;
    }
    if (!host->WaitSerial(serial, timeoutMilliseconds)) {
        Fail(Status::NotReady, error,
             "surface drain did not complete within the timeout");
        return false;
    }
    return true;
}

Status d3d12::detail::Q1bState::CreateSurface(const SurfaceDesc& desc, Surface* surface,
                                             std::string* error) {
    if (desc.width == 0 || desc.height == 0) {
        return Fail(Status::InvalidArgument, error, "surface size is zero");
    }
    if (desc.width > 16384 || desc.height > 16384) {
        return Fail(Status::InvalidArgument, error, "surface size exceeds 16384");
    }
    if (desc.title.empty()) {
        return Fail(Status::InvalidArgument, error, "surface title is empty");
    }
    ComPtr<IDXGIFactory2> factory;
    if (host->Factory() == nullptr ||
        FAILED(host->Factory()->QueryInterface(
            __uuidof(IDXGIFactory2), reinterpret_cast<void**>(factory.GetAddressOf()))) ||
        !factory) {
        return Fail(Status::Unsupported, error,
                    "the installed display factory cannot create a window surface");
    }

    SurfaceRecord record;
    record.id = nextSurfaceId++;
    record.className = L"DOOM3Q1bSurface" +
                       std::to_wstring(gWindowClassSerial.fetch_add(1));
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = SurfaceWindowProc;
    windowClass.hInstance = GetModuleHandleW(nullptr);
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = record.className.c_str();
    if (RegisterClassExW(&windowClass) == 0) {
        return Fail(Status::Error, error,
                    "RegisterClassExW failed with " +
                        d3d12::detail::HrString(HRESULT_FROM_WIN32(GetLastError())));
    }
    RECT rect{0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    const std::wstring title = Widen(desc.title);
    record.window = CreateWindowExW(
        0, record.className.c_str(), title.c_str(), WS_OVERLAPPEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr,
        windowClass.hInstance, nullptr);
    if (record.window == nullptr) {
        UnregisterClassW(record.className.c_str(), windowClass.hInstance);
        return Fail(Status::Error, error,
                    "CreateWindowExW failed with " +
                        d3d12::detail::HrString(HRESULT_FROM_WIN32(GetLastError())));
    }
    ShowWindow(record.window, SW_SHOW);
    UpdateWindow(record.window);

    DXGI_SWAP_CHAIN_DESC1 chainDescription{};
    chainDescription.Width = desc.width;
    chainDescription.Height = desc.height;
    chainDescription.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    chainDescription.Stereo = FALSE;
    chainDescription.SampleDesc.Count = 1;
    chainDescription.SampleDesc.Quality = 0;
    chainDescription.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    chainDescription.BufferCount = 2;
    chainDescription.Scaling = DXGI_SCALING_STRETCH;
    chainDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    chainDescription.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
    chainDescription.Flags = 0;
    ComPtr<IDXGISwapChain1> chain1;
    const HRESULT chainResult = factory->CreateSwapChainForHwnd(
        host->Queue(), record.window, &chainDescription, nullptr, nullptr,
        chain1.GetAddressOf());
    if (FAILED(chainResult)) {
        DestroyWindow(record.window);
        UnregisterClassW(record.className.c_str(), windowClass.hInstance);
        return Fail(chainResult == DXGI_ERROR_UNSUPPORTED ? Status::Unsupported
                                                          : Status::Error,
                    error, "CreateSwapChainForHwnd " +
                               d3d12::detail::HrString(chainResult));
    }
    if (FAILED(chain1.As(&record.chain)) || !record.chain) {
        DestroyWindow(record.window);
        UnregisterClassW(record.className.c_str(), windowClass.hInstance);
        return Fail(Status::Error, error, "the created chain has no frame index");
    }
    D3D12_DESCRIPTOR_HEAP_DESC heapDescription{};
    heapDescription.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    heapDescription.NumDescriptors = 2;
    heapDescription.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    HRESULT hr = host->Device()->CreateDescriptorHeap(
        &heapDescription, __uuidof(ID3D12DescriptorHeap),
        reinterpret_cast<void**>(record.viewHeap.GetAddressOf()));
    if (FAILED(hr)) {
        DestroyWindow(record.window);
        UnregisterClassW(record.className.c_str(), windowClass.hInstance);
        return Fail(Status::Error, error,
                    "CreateDescriptorHeap(surface) " + d3d12::detail::HrString(hr));
    }
    record.width = desc.width;
    record.height = desc.height;
    record.historyGeneration = 1;
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        record.viewHeap->GetCPUDescriptorHandleForHeapStart();
    const UINT increment =
        host->Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    record.backBuffers.resize(2);
    for (uint32_t i = 0; i < 2; ++i) {
        hr = record.chain->GetBuffer(i, __uuidof(ID3D12Resource),
                                     reinterpret_cast<void**>(
                                         record.backBuffers[i].GetAddressOf()));
        if (FAILED(hr)) {
            DestroyWindow(record.window);
            UnregisterClassW(record.className.c_str(), windowClass.hInstance);
            return Fail(Status::Error, error,
                        "SwapChain::GetBuffer " + d3d12::detail::HrString(hr));
        }
        host->Device()->CreateRenderTargetView(record.backBuffers[i].Get(), nullptr,
                                              handle);
        handle.ptr += increment;
    }
    surface->id = record.id;
    surfaces.emplace(record.id, std::move(record));
    if (error) {
        error->clear();
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::SubmitSurfaceFrame(const Surface& surface,
                                                  const ClearRow& color,
                                                  Submission* submission,
                                                  std::string* error) {
    auto entry = surfaces.find(surface.id);
    if (entry == surfaces.end()) {
        return Fail(Status::InvalidArgument, error, "unknown surface");
    }
    SurfaceRecord& target = entry->second;
    if (!target.chain || !target.window) {
        return Fail(Status::Error, error, "surface has no display chain");
    }
    PumpMessages();
    if (IsIconic(target.window)) {
        target.minimized = true;
        return Fail(Status::NotReady, error,
                    "surface is minimized; no frame was composed");
    }
    target.minimized = false;
    uint64_t completedAtAcquire = 0;
    FrameSlot* slot = nullptr;
    const Status acquired = AcquireSlot(&slot, &completedAtAcquire, error);
    if (acquired != Status::Ok) {
        return acquired;
    }
    const uint32_t index = target.chain->GetCurrentBackBufferIndex();
    if (index >= target.backBuffers.size()) {
        return Fail(Status::Error, error, "surface frame index is out of range");
    }
    ComPtr<ID3D12Resource> backBuffer = target.backBuffers[index];
    if (!backBuffer) {
        return Fail(Status::Error, error,
                    "surface back buffer is unavailable after a failed resize");
    }
    D3D12_RESOURCE_DESC backDescription = backBuffer->GetDesc();
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
    UINT rows = 0;
    UINT64 rowBytes = 0;
    UINT64 totalBytes = 0;
    host->Device()->GetCopyableFootprints(&backDescription, 0, 1, 0, &footprint, &rows,
                                         &rowBytes, &totalBytes);
    if (footprint.Footprint.RowPitch < target.width * 4 || rows != target.height) {
        return Fail(Status::Error, error,
                    "surface readback layout is not the expected tight texture");
    }

    d3d12::detail::SubmissionRecord record;
    if (!d3d12::detail::AllocateSubmissionId(&record.id) ||
        !d3d12::detail::AllocateCompletionToken(&record.token)) {
        return Fail(Status::Error, error, "submission identity space exhausted");
    }
    record.serial = host->NextSerial();
    if (record.serial == 0) {
        return Fail(Status::Error, error, "completion value space exhausted");
    }
    record.width = target.width;
    record.height = target.height;
    record.slotIndex = slot->index;
    record.slotCompletedAtAcquire = completedAtAcquire;
    record.attachmentCount = 1;
    record.historyGeneration = static_cast<uint32_t>(target.historyGeneration);
    record.colorStrides.push_back(target.width * 4);
    record.colorPitches.push_back(footprint.Footprint.RowPitch);
    D3D12_RESOURCE_DESC readbackDescription{};
    readbackDescription.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    readbackDescription.Alignment = 0;
    readbackDescription.Width = totalBytes;
    readbackDescription.Height = 1;
    readbackDescription.DepthOrArraySize = 1;
    readbackDescription.MipLevels = 1;
    readbackDescription.Format = DXGI_FORMAT_UNKNOWN;
    readbackDescription.SampleDesc.Count = 1;
    readbackDescription.SampleDesc.Quality = 0;
    readbackDescription.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    readbackDescription.Flags = D3D12_RESOURCE_FLAG_NONE;
    D3D12_HEAP_PROPERTIES readbackHeap{};
    readbackHeap.Type = D3D12_HEAP_TYPE_READBACK;
    readbackHeap.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    readbackHeap.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    readbackHeap.CreationNodeMask = 1;
    readbackHeap.VisibleNodeMask = 1;
    ComPtr<ID3D12Resource> readback;
    const HRESULT readbackResult = host->Device()->CreateCommittedResource(
        &readbackHeap, D3D12_HEAP_FLAG_NONE, &readbackDescription,
        D3D12_RESOURCE_STATE_COPY_DEST, nullptr, __uuidof(ID3D12Resource),
        reinterpret_cast<void**>(readback.GetAddressOf()));
    if (FAILED(readbackResult)) {
        return Fail(Status::Error, error,
                    "CreateCommittedResource(surface readback) " +
                        d3d12::detail::HrString(readbackResult));
    }
    record.colorReadbacks.push_back(readback);

    ID3D12GraphicsCommandList* list = slot->list.Get();
    ChangeUse(list, backBuffer.Get(), D3D12_RESOURCE_STATE_PRESENT,
              D3D12_RESOURCE_STATE_RENDER_TARGET);
    D3D12_CPU_DESCRIPTOR_HANDLE view =
        target.viewHeap->GetCPUDescriptorHandleForHeapStart();
    view.ptr += static_cast<SIZE_T>(index) *
                host->Device()->GetDescriptorHandleIncrementSize(
                    D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    const FLOAT clear[4] = {color.red / 255.0f, color.green / 255.0f,
                            color.blue / 255.0f, color.alpha / 255.0f};
    list->ClearRenderTargetView(view, clear, 0, nullptr);
    ChangeUse(list, backBuffer.Get(), D3D12_RESOURCE_STATE_RENDER_TARGET,
              D3D12_RESOURCE_STATE_COPY_SOURCE);
    D3D12_TEXTURE_COPY_LOCATION destination{};
    destination.pResource = readback.Get();
    destination.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
    destination.PlacedFootprint = footprint;
    D3D12_TEXTURE_COPY_LOCATION source{};
    source.pResource = backBuffer.Get();
    source.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
    source.SubresourceIndex = 0;
    list->CopyTextureRegion(&destination, 0, 0, 0, &source, nullptr);
    ChangeUse(list, backBuffer.Get(), D3D12_RESOURCE_STATE_COPY_SOURCE,
              D3D12_RESOURCE_STATE_PRESENT);
    const HRESULT close = list->Close();
    if (FAILED(close)) {
        return Fail(Status::Error, error,
                    "ID3D12GraphicsCommandList::Close(surface) " +
                        d3d12::detail::HrString(close));
    }
    record.journal.push_back(d3d12::JournalEntry{
        "surface-backbuffer", "PRESENT", "COPY_SOURCE",
        "cleared, copied to readback, returned"});
    d3d12::detail::SubmissionRecord* storedPtr =
        host->AddHostRecord(std::move(record), error);
    if (storedPtr == nullptr) {
        return Fail(Status::Error, error, "submission identity collision");
    }
    d3d12::detail::SubmissionRecord& stored = *storedPtr;
    FinishSlot(*slot, stored.serial);
    ID3D12CommandList* lists[] = {list};
    host->Queue()->ExecuteCommandLists(1, lists);
    const HRESULT signalled =
        host->Queue()->Signal(host->Completion(), stored.serial);
    if (FAILED(signalled)) {
        stored.signalled = false;
        return Fail(Status::Error, error,
                    "CommandQueue::Signal(surface) " +
                        d3d12::detail::HrString(signalled));
    }
    stored.signalled = true;
    const HRESULT shown = target.chain->Present(1, 0);
    if (FAILED(shown)) {
        return Fail(shown == DXGI_ERROR_DEVICE_REMOVED ? Status::DeviceLost
                                                       : Status::Error,
                    error, "SwapChain::Present " + d3d12::detail::HrString(shown));
    }
    submission->id = stored.id;
    submission->completionToken = stored.token;
    if (error) {
        error->clear();
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::ResizeSurface(const Surface& surface, uint32_t width,
                                             uint32_t height, std::string* error) {
    auto entry = surfaces.find(surface.id);
    if (entry == surfaces.end()) {
        return Fail(Status::InvalidArgument, error, "unknown surface");
    }
    SurfaceRecord& target = entry->second;
    if (width == 0 || height == 0 || width > 16384 || height > 16384) {
        return Fail(Status::InvalidArgument, error,
                    "requested surface size is outside 1..16384");
    }
    if (IsIconic(target.window)) {
        return Fail(Status::NotReady, error,
                    "surface is minimized; restore it before resizing");
    }
    if (!DrainQueue(10000, error)) {
        return Status::Error;
    }
    // A closed command list still holds a reference to every resource it
    // recorded. ResizeBuffers rejects the call while any such reference is
    // outstanding, so discard each list's recorded references by resetting it
    // against a freshly reset allocator and closing it again. DrainQueue above
    // proves the GPU is done with all of it.
    for (FrameSlot& slot : slots) {
        if (!slot.allocator || !slot.list) {
            continue;
        }
        const HRESULT allocatorReset = slot.allocator->Reset();
        if (FAILED(allocatorReset)) {
            return Fail(Status::Error, error,
                        "CommandAllocator::Reset(surface resize) " +
                            d3d12::detail::HrString(allocatorReset));
        }
        const HRESULT listReset = slot.list->Reset(slot.allocator.Get(), nullptr);
        if (FAILED(listReset)) {
            return Fail(Status::Error, error,
                        "GraphicsCommandList::Reset(surface resize) " +
                            d3d12::detail::HrString(listReset));
        }
        const HRESULT listClose = slot.list->Close();
        if (FAILED(listClose)) {
            return Fail(Status::Error, error,
                        "GraphicsCommandList::Close(surface resize) " +
                            d3d12::detail::HrString(listClose));
        }
    }
    for (ComPtr<ID3D12Resource>& buffer : target.backBuffers) {
        buffer.Reset();
    }
    const HRESULT resized =
        target.chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
    if (FAILED(resized)) {
        return Fail(Status::Error, error,
                    "SwapChain::ResizeBuffers " + d3d12::detail::HrString(resized));
    }
    D3D12_CPU_DESCRIPTOR_HANDLE handle =
        target.viewHeap->GetCPUDescriptorHandleForHeapStart();
    const UINT increment =
        host->Device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for (uint32_t i = 0; i < 2; ++i) {
        const HRESULT got =
            target.chain->GetBuffer(i, __uuidof(ID3D12Resource),
                                    reinterpret_cast<void**>(
                                        target.backBuffers[i].GetAddressOf()));
        if (FAILED(got)) {
            return Fail(Status::Error, error,
                        "SwapChain::GetBuffer(after resize) " +
                            d3d12::detail::HrString(got));
        }
        host->Device()->CreateRenderTargetView(target.backBuffers[i].Get(), nullptr,
                                              handle);
        handle.ptr += increment;
    }
    RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    SetWindowPos(target.window, nullptr, 0, 0, rect.right - rect.left,
                 rect.bottom - rect.top, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);
    target.width = width;
    target.height = height;
    ++target.historyGeneration;
    if (error) {
        error->clear();
    }
    return Status::Ok;
}

Status d3d12::detail::Q1bState::DestroySurface(const Surface& surface,
                                              std::string* error) {
    auto entry = surfaces.find(surface.id);
    if (entry == surfaces.end()) {
        return Fail(Status::InvalidArgument, error, "unknown or destroyed surface");
    }
    // A frame that is still in flight may reference the chain buffers.
    if (!DrainQueue(10000, error)) {
        return Status::Error;
    }
    const std::wstring className = entry->second.className;
    HWND window = entry->second.window;
    entry->second.backBuffers.clear();
    entry->second.chain.Reset();
    entry->second.viewHeap.Reset();
    surfaces.erase(entry);
    if (window != nullptr) {
        DestroyWindow(window);
    }
    if (!className.empty()) {
        UnregisterClassW(className.c_str(), GetModuleHandleW(nullptr));
    }
    if (error) {
        error->clear();
    }
    return Status::Ok;
}

}  // namespace rhi
}  // namespace doom3
