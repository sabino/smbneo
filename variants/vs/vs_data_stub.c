/*
 * ROM-free placeholders for the browser build.
 *
 * The site patches these exact arrays with bytes from the user's local
 * suprmrio.zip.  Keep their types and sizes identical to gen_vs_assets.py.
 */
#include <stdint.h>

#if defined(__GNUC__) && !defined(__clang__)
#define SMBNEO_TEMPLATE_DATA __attribute__((used, externally_visible))
#elif defined(__GNUC__) || defined(__clang__)
#define SMBNEO_TEMPLATE_DATA __attribute__((used))
#else
#define SMBNEO_TEMPLATE_DATA
#endif

_Alignas(2) const uint8_t vs_prg[32768] SMBNEO_TEMPLATE_DATA = {0};
_Alignas(2) const uint8_t vs_chr[16384] SMBNEO_TEMPLATE_DATA = {0};
const uint16_t vs_palette_neogeo[64] SMBNEO_TEMPLATE_DATA = {0};
