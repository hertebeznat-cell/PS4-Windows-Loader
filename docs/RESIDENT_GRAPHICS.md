# Conditional resident graphics and text output

The closed resident image now includes GOP QueryMode/SetMode/Blt at indices
54..56 and all nine SimpleTextOutput methods at 57..65. No console is published
by default. `pwl_graphics_prepare` builds AMD64 protocol/mode layouts for an
explicit existing linear framebuffer, without touching video memory.

The framebuffer must have a supplied physical address, width, height, stride
in pixels, byte extent, RGB/BGR 32-bit format and actual PAT index. Geometry and
overflow are validated; guessing a PS4 resolution, deriving a framebuffer from
a screenshot, or treating tiled GPU memory as linear is not supported.

`pwl_native_graphics_publish` checks the audited resident/transition environment,
rejects overlap with either owned arena or table pool, verifies every framebuffer
page as identity mapped supervisor RW/NX with the supplied PAT, and reserves two
protocol entries before modifying anything. It stores the explicit source spec
in the preparation owner, publishes GOP and SimpleTextOutput on one handle,
sets ConOut/StdErr and updates the SystemTable CRC. Later environment/entry
audits recheck the retained geometry, destination pointers, interfaces, CRC and
framebuffer mappings. The publisher does not discover hardware, create its
MMIO mappings, initialize a GPU, change video registers or supply RAM/MMIO
inventory. Ownership, a real linear mode, cacheability and device stability
are explicit platform inputs, not established by passing a geometry check.

GOP exposes one already active mode. QueryMode returns an allocated independent
information snapshot, freeable with FreePool. SetMode(0) clears visible pixels
without changing hardware. Blt implements fill, video-to-buffer,
buffer-to-video and overlap-safe video-to-video operations. It validates video
coordinates, byte stride and buffer arithmetic, translates RGB/BGR channels,
and leaves pitch padding untouched. A BLT buffer aliasing video memory is
refused for buffer transfers; use VideoToVideo for video overlap. The caller
owns sufficient accessible BLT-buffer memory, as the EFI API has no buffer-size
argument. The framebuffer uses volatile accesses.

SimpleTextOutput supplies mode 0 at 80x25, a double-height 8x8 Basic Latin font,
foreground/background EFI colors, clearing, reset, cursor positioning/visibility,
CR/LF/backspace, wrapping and scrolling. The viewport needs at least 640x400.
Unknown UTF-16 glyphs render '?' and return EFI_WARN_UNKNOWN_GLYPH; TestString
refuses them. Additional Unicode fonts, wide/narrow glyph controls and extra
text modes are absent. SimpleTextInput/keyboard is not produced. The font is
the public-domain [font8x8_basic](https://github.com/dhepper/font8x8/blob/master/font8x8_basic.h)
by Daniel Hepper, based on Marcel Sondaar/IBM VGA glyphs; its attribution and
license notice are retained in `loader/src/console_font.h`.

Native host tests exercise actual copied RX callbacks in both channel formats,
stride padding, all four BLT operations, horizontal/vertical overlap, allocated
QueryMode buffers, text rendering, unknown glyphs, attributes, cursor, bottom
row wrap/scroll, invalid extents and guard pages. Workspace tests check
transactional publication, wrong PAT refusal, exact console pointer/CRC audit
and later entry validation. These are real CPU calls, not an emulator or proof
that a PS4 linear framebuffer has been located. No Microsoft code executes.

The PS4 framebuffer address, format, linearity and ownership have not been
captured by this project. Accordingly the actual console backend remains
disabled in normal builds. The existing Stage 4.8 path is still preflight.

Primary ABI: [UEFI console protocols](https://uefi.org/specs/UEFI/2.10/12_Protocols_Console_Support.html).
