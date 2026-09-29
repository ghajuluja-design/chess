            hoverSq = squareFromPoint(x, y);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONDOWN: {
        int x = short(LOWORD(lParam)), y = short(HIWORD(lParam));
        if (promoFrom >= 0) {
            POINT pt = {x, y};
            for (int i = 0; i < 4; i++) {
                if (PtInRect(&promoRects[i], pt)) {
                    const char types[4] = {'q', 'r', 'b', 'n'};
                    for (auto& m : legal)
                        if (m.from == promoFrom && m.to == promoTo && m.promo == types[i]) {
                            Move chosen = m;
                            promoFrom = promoTo = -1;
                            doMove(chosen);
                            return 0;
                        }
                }
            }
            return 0;  // click outside picker ignored
        }
        if (gameOver) return 0;
        int s = squareFromPoint(x, y);
        if (s < 0) return 0;
        auto p = current.board[s];
        if (p && p->color == current.turn) {
            dragging = true;
            dragFrom = s;
            dragPos = {x, y};
            hoverSq = s;
            dragMoves.clear();
            for (auto& m : legal) if (m.from == s) dragMoves.push_back(m);
            SetCapture(hwnd);
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;
    }

    case WM_LBUTTONUP: {
        if (!dragging) return 0;
        ReleaseCapture();
        int x = short(LOWORD(lParam)), y = short(HIWORD(lParam));
        int s = squareFromPoint(x, y);
        dragging = false;
        if (s >= 0 && s != dragFrom) {
            std::vector<Move> matching;
            for (auto& m : dragMoves) if (m.to == s) matching.push_back(m);
            if (matching.size() == 1) {
                Move mv = matching[0];
                dragFrom = -1;
                dragMoves.clear();
                doMove(mv);
                return 0;
            } else if (matching.size() > 1) {  // promotion choice
                promoFrom = dragFrom;
                promoTo = s;
                promoColor = current.turn;
                dragFrom = -1;
                dragMoves.clear();
                InvalidateRect(hwnd, nullptr, TRUE);
                return 0;
            }
        }
        dragFrom = -1;
        dragMoves.clear();
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;
    }

    case WM_RBUTTONDOWN:
        dragging = false;
        dragFrom = -1;
        dragMoves.clear();
        promoFrom = promoTo = -1;
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_KEYDOWN:
        if (wParam == 'N') { newGame(); InvalidateRect(hwnd, nullptr, TRUE); }
        else if (wParam == 'U') undo();
        return 0;

    case WM_DESTROY:
        DeleteObject(fontPieces);
        DeleteObject(fontUI);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

int WINAPI wWinMain(HINSTANCE hInst, HINSTANCE, PWSTR, int nShow) {
    newGame();

    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInst;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = nullptr;
    wc.lpszClassName = L"ChessDragDrop";
    wc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wc);

    RECT wr = {0, 0, WIN_W, WIN_H};
    AdjustWindowRect(&wr, WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, FALSE);
    HWND hwnd = CreateWindowExW(0, wc.lpszClassName, L"Chess - drag pieces (N=new, U=undo)",
                                WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                wr.right - wr.left, wr.bottom - wr.top,
                                nullptr, nullptr, hInst, nullptr);
    g_hwnd = hwnd;
    ShowWindow(hwnd, nShow);
    UpdateWindow(hwnd);

    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }
    return int(m.wParam);
}
+92
-0
Add-Type @"
using System;
using System.Runtime.InteropServices;
public class Win32Sim {
    [DllImport("user32.dll")] public static extern IntPtr FindWindowW(string cls, string title);
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ClientToScreen(IntPtr h, ref POINT p);
    [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
    [StructLayout(LayoutKind.Sequential)] public struct POINT { public int x, y; }
    [StructLayout(LayoutKind.Sequential)] public struct RECT { public int left, top, right, bottom; }
    [StructLayout(LayoutKind.Sequential)] public struct INPUT {
        public uint type;
        public INPUTUNION u;
    }
    [StructLayout(LayoutKind.Explicit)] public struct INPUTUNION {
        [FieldOffset(0)] public MOUSEINPUT mi;
    }
    [StructLayout(LayoutKind.Sequential)] public struct MOUSEINPUT {
        public int dx, dy;
        public uint mouseData, dwFlags, time;
        public IntPtr dwExtraInfo;
    }
    [DllImport("user32.dll")] public static extern uint SendInput(uint n, INPUT[] p, int size);
    public const uint MOUSEEVENTF_MOVE = 0x0001;
    public const uint MOUSEEVENTF_LEFTDOWN = 0x0002;
    public const uint MOUSEEVENTF_LEFTUP = 0x0004;
    public const uint MOUSEEVENTF_ABSOLUTE = 0x8000;
    public const uint INPUT_MOUSE = 0;
    public static void MouseTo(int sx, int sy) {
        int vsx = sx * 65535 / (GetSystemMetrics(0) - 1);
        int vsy = sy * 65535 / (GetSystemMetrics(1) - 1);
        INPUT i = new INPUT(); i.type = INPUT_MOUSE;
        i.u.mi.dx = vsx; i.u.mi.dy = vsy; i.u.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE;
        SendInput(1, new INPUT[]{i}, Marshal.SizeOf(typeof(INPUT)));
    }
    public static void Click(int sx, int sy, bool down) {
        INPUT i = new INPUT(); i.type = INPUT_MOUSE;
        i.u.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
        SendInput(1, new INPUT[]{i}, Marshal.SizeOf(typeof(INPUT)));
    }
    [DllImport("user32.dll")] public static extern int GetSystemMetrics(int idx);
    [DllImport("user32.dll")] public static extern void keybd_event(byte vk, byte scan, uint flags, IntPtr extra);
}
"@

$proc = Get-Process chess_gui -ErrorAction SilentlyContinue | Select-Object -First 1
if (-not $proc -or $proc.MainWindowHandle -eq [IntPtr]::Zero) { Write-Output "window not found"; exit 1 }
$h = $proc.MainWindowHandle
[Win32Sim]::SetForegroundWindow($h) | Out-Null
Start-Sleep -Milliseconds 300

# reset game with 'N'
[Win32Sim]::keybd_event(0x4E, 0, 0, [IntPtr]::Zero)
[Win32Sim]::keybd_event(0x4E, 0, 2, [IntPtr]::Zero)
Start-Sleep -Milliseconds 300

# client coords: board origin (28,28), square 64. e2 center = (28+4*64+32, 28+(7-1)*64+32) = (316, 444)
# e4 center = (316, 28+(7-3)*64+32) = (316, 316)
$p = New-Object Win32Sim+POINT; $p.x = 316; $p.y = 444
[Win32Sim]::ClientToScreen($h, [ref]$p) | Out-Null
$ex = $p.x; $ey = $p.y
$p2 = New-Object Win32Sim+POINT; $p2.x = 316; $p2.y = 316
[Win32Sim]::ClientToScreen($h, [ref]$p2) | Out-Null
$tx = $p2.x; $ty = $p2.y

[Win32Sim]::MouseTo($ex, $ey)
Start-Sleep -Milliseconds 100
[Win32Sim]::Click($ex, $ey, $true)
Start-Sleep -Milliseconds 100