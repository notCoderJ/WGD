#include "hangul.h"
#include "ASCFont.h"
#include "KSFont.h"
#include "intf/ssd1306_interface.h"

static uint8_t ssd1306_invt_byte = 0x00000000;
byte HANFontImage[32] = {0, };

static byte* getHAN_font(byte HAN1, byte HAN2, byte HAN3);
static void lcd_draw_bitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *buf);

void matrixPrint (int XPOS, int YPOS, char* pChar) {
	byte *pFs;
	byte c, c2, c3;

	while(*pChar) { 
		c = *(byte*)pChar++;

		//---------- 한글 ---------
		if(c >= 0x80) {
			c2 = *(byte*)pChar++;
			c3 = *(byte*)pChar++;
			pFs = getHAN_font(c, c2, c3);
			lcd_draw_bitmap(XPOS, YPOS, 16, 16, pFs);
			XPOS = XPOS+16;
			if(XPOS > 128){
				XPOS = 0;
				YPOS = YPOS+16;
			}
		}
		//---------- ASCII ---------
		else {
			pFs = (byte*)ASCfontSet + ((byte)c - 0x20) * 16;
			lcd_draw_bitmap(XPOS, YPOS, 8, 16, pFs);
			XPOS = XPOS+8;
			if(XPOS > 128){
				XPOS = 0;
				YPOS = YPOS+16;
			}   
		}   
	}  
}  

/*=============================================================================
  한글 font 처리부분 
  =============================================================================*/

static byte* getHAN_font(byte HAN1, byte HAN2, byte HAN3) {

	const byte cho[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 3, 3, 3, 1, 2, 4, 4, 4, 2, 1, 3, 0 };
	const byte cho2[] = { 0, 5, 5, 5, 5, 5, 5, 5, 5, 6, 7, 7, 7, 6, 6, 7, 7, 7, 6, 6, 7, 5 };
	const byte jong[] = { 0, 0, 2, 0, 2, 1, 2, 1, 2, 3, 0, 2, 1, 3, 3, 1, 2, 1, 3, 3, 1, 1 };

	uint16_t utf16;
	byte first, mid, last;
	byte firstType, midType, lastType;
	byte i;
	byte *pB, *pF;

	/*------------------------------
	  UTF-8 을 UTF-16으로 변환한다.

	  UTF-8 1110xxxx 10xxxxxx 10xxxxxx
	  ------------------------------*/
	utf16 = (HAN1 & 0x0f) << 12 | (HAN2 & 0x3f) << 6 | HAN3 & 0x3f;

	/*------------------------------
	  초,중,종성 코드를 분리해 낸다.

	  unicode = {[(초성 * 21) + 중성] * 28}+ 종성 + 0xAC00

	  0   1  2  3  4  5  6  7  8  9  10 11 12 13 14 15 16 17 18 19 20 21 22 23 24 25 26 27 
	  초성 ㄱ   ㄲ ㄴ ㄷ ㄸ ㄹ ㅁ ㅂ ㅃ ㅅ ㅆ ㅇ ㅈ ㅉ ㅊ ㅋ ㅌ ㅍ ㅎ
	  중성 ㅏ   ㅐ ㅑ ㅒ ㅓ ㅔ ㅕ ㅖ ㅗ ㅘ ㅙ ㅚ ㅛ ㅜ ㅝ ㅞ ㅟ ㅠ ㅡ ㅢ ㅣ  
	  종성 없음 ㄱ ㄲ ㄳ ㄴ ㄵ ㄶ ㄷ ㄹ ㄺ ㄻ ㄼ ㄽ ㄾ ㄿ ㅀ ㅁ ㅂ ㅄ ㅅ ㅆ ㅇ ㅈ ㅊ ㅋ ㅌ ㅍ ㅎ
	  ------------------------------*/
	utf16 -= 0xac00;
	last = utf16 % 28;
	utf16 /= 28;
	mid = utf16 % 21;
	first = utf16 / 21;

	first++;
	mid++;

	/*------------------------------
	  초,중,종성 해당 폰트 타입(벌)을 결정한다.
	  ------------------------------*/

	/*
	   초성 19자:ㄱㄲㄴㄷㄸㄹㅁㅂㅃㅅㅆㅇㅈㅉㅊㅋㅌㅍㅎ
	   중성 21자:ㅏㅐㅑㅒㅓㅔㅕㅖㅗㅘㅙㅚㅛㅜㅝㅞㅟㅠㅡㅢㅣ
	   종성 27자:ㄱㄲㄳㄴㄵㄶㄷㄹㄺㄻㄼㄽㄾㄿㅀㅁㅂㅄㅆㅇㅈㅊㅋㅌㅍㅎ

	   초성
	   초성 1벌 : 받침없는 'ㅏㅐㅑㅒㅓㅔㅕㅖㅣ' 와 결합
	   초성 2벌 : 받침없는 'ㅗㅛㅡ'
	   초성 3벌 : 받침없는 'ㅜㅠ'
	   초성 4벌 : 받침없는 'ㅘㅙㅚㅢ'
	   초성 5벌 : 받침없는 'ㅝㅞㅟ'
	   초성 6벌 : 받침있는 'ㅏㅐㅑㅒㅓㅔㅕㅖㅣ' 와 결합
	   초성 7벌 : 받침있는 'ㅗㅛㅜㅠㅡ'
	   초성 8벌 : 받침있는 'ㅘㅙㅚㅢㅝㅞㅟ'

	   중성
	   중성 1벌 : 받침없는 'ㄱㅋ' 와 결합
	   중성 2벌 : 받침없는 'ㄱㅋ' 이외의 자음
	   중성 3벌 : 받침있는 'ㄱㅋ' 와 결합
	   중성 4벌 : 받침있는 'ㄱㅋ' 이외의 자음

	   종성
	   종성 1벌 : 중성 'ㅏㅑㅘ' 와 결합
	   종성 2벌 : 중성 'ㅓㅕㅚㅝㅟㅢㅣ'
	   종성 3벌 : 중성 'ㅐㅒㅔㅖㅙㅞ'
	   종성 4벌 : 중성 'ㅗㅛㅜㅠㅡ'

*/
	if(!last) {  //받침 없는 경우
		firstType = cho[mid];
		if(first == 1 || first == 24) midType = 0;
		else midType = 1;
	}
	else {  //받침 있는 경우
		firstType = cho2[mid];
		if(first == 1 || first == 24) midType = 2;
		else midType = 3;
		lastType = jong[mid];
	}
	memset(HANFontImage, 0, 32);

	//초성 
	pB = HANFontImage;
	pF = (byte*)KSFont + (firstType*20 + first)*32;
	i = 32;
	while (i--) { *pB++ = pgm_read_byte(pF++); }

	//중성
	pB = HANFontImage;
	pF = (byte*)KSFont + (8*20 + midType*22 + mid)*32;
	i = 32;
	while (i--) { *pB++ |= pgm_read_byte(pF++); }

	//종성
	if (last) {
		pB = HANFontImage;
		pF = (byte*)KSFont + (8*20 + 4*22 + lastType*28 + last)*32;
		i = 32;
		while (i--) { *pB++ |= pgm_read_byte(pF++); }
	}

	return HANFontImage;
}

static void lcd_draw_bitmap(uint8_t x, uint8_t y, uint8_t w, uint8_t h, const uint8_t *buf) {
	uint8_t i, j;
	lcduint_t pitch = (w + 7) >> 3;
	ssd1306_lcd.set_block(x, y, w);

	for (j=(h>>3); j>0; j--)
	{
		uint8_t bit = 8;
		for (i=w; i>0; i--)
		{
			uint8_t data = 0;
			for (uint8_t k = 0; k<8; k++)
			{
				data |= (((pgm_read_byte(&buf[k*pitch]) >> bit) & 0x01) << k);
			}

			ssd1306_lcd.send_pixels1(ssd1306_invt_byte^data);
			bit--;

			if (bit < 1)
			{
				buf++;
				bit=7;
			}
		}
		if (!bit)
		{
			buf++;
		}
		buf += pitch * 7;
		ssd1306_lcd.next_page();
	}
	ssd1306_intf.stop();
}
