#ifndef PICO8_CHARSET_H
#define PICO8_CHARSET_H

/*
 * pico8_charset.h — between PICO-8's own font codes and UTF-8.
 *
 * Inside the VM everything is a charset byte: PICO-8 keeps the codes of
 * its own font and only converts at the edges, where the text becomes
 * UTF-8 - in a cart file, in the shell, or on the host screen.
 *
 * VM 안에서는 모든 것이 charset 바이트다. PICO-8은 자기 폰트의 코드를 그대로
 * 쓰고, 텍스트가 UTF-8이 되는 경계(카트 파일, 셸, 호스트 화면)에서만 변환한다.
 */

#include <string>

#include "pico8_core.h"

/*
 * Convert UTF-8 text into the charset bytes PICO-8 uses.
 *
 * Characters PICO-8 has no glyph for are replaced by the "unknown" glyph,
 * the same way the real PICO-8 does it.
 *
 * UTF-8 텍스트를 PICO-8이 쓰는 charset 바이트로 바꾼다.
 * PICO-8에 글자가 없는 문자는 실제 PICO-8과 마찬가지로 "모르는 글자"로 바뀐다.
 */
std::string pico8_charset_from_utf8(const char *src, size_t len);

/*
 * Convert charset bytes back into UTF-8 text.
 *
 * This is the direction the shell and the cart files use.
 *
 * charset 바이트를 다시 UTF-8 텍스트로 바꾼다.
 * 셸과 카트 파일이 쓰는 방향이다.
 */
std::string pico8_charset_to_utf8(const char *src, size_t len);

#endif /* PICO8_CHARSET_H */
