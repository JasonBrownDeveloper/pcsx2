/*
 *	Copyright (C) 2014-2014 Gregory hainaut
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with GNU Make; see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "GSOsdManager.h"

void GSOsdManager::LoadFont() {
	FT_Error error = FT_New_Face(m_library, m_font.c_str(), 0, &m_face);
	if (error == FT_Err_Unknown_File_Format) {
		fprintf(stderr, "Failed to init the freetype face: unknwon format\n");
		return;
	} else if (error) {
		fprintf(stderr, "Failed to init the freetype face\n");
		return;
	}

	LoadSize();
}

void GSOsdManager::LoadSize() {
	FT_Error error = FT_Set_Pixel_Sizes(m_face, 0, m_size);;
	if (error) {
		fprintf(stderr, "Failed to init the face size\n");
		return;
	}

	memset(c_info, 0, sizeof(c_info));
	atlas_w = 0;
	atlas_h = 0;

	for(uint32 i = ascii_start; i < ascii_stop; i++) {
		if(FT_Load_Char(m_face, i, FT_LOAD_RENDER))
			continue;

		// Size of char
		c_info[i].ax = m_face->glyph->advance.x >> 6;
		c_info[i].ay = m_face->glyph->advance.y >> 6;

		c_info[i].bw = m_face->glyph->bitmap.width;
		c_info[i].bh = m_face->glyph->bitmap.rows;

		c_info[i].bl = m_face->glyph->bitmap_left;
		c_info[i].bt = m_face->glyph->bitmap_top;

		// Size of atlas
		atlas_w += m_face->glyph->bitmap.width;
		atlas_h = std::max(atlas_h, (uint32)m_face->glyph->bitmap.rows);
	}
}

GSOsdManager::GSOsdManager() : m_font("/usr/share/fonts/truetype/freefont/FreeMono.ttf")
                             , m_size(48)
                             , atlas_h(0)
                             , atlas_w(0)
                             , ascii_start(32)
                             , ascii_stop(128)
{
	if (FT_Init_FreeType(&m_library)) {
		fprintf(stderr, "Failed to init the freetype library\n");
		return;
	}

	LoadFont();
}

GSOsdManager::~GSOsdManager() {
	FT_Done_FreeType(m_library);
}

GSVector2i GSOsdManager::get_texture_font_size() {
	return GSVector2i(atlas_w, atlas_h);
}

void GSOsdManager::upload_texture_atlas(GSTexture* t) {
	int x = 0;
	for(uint32 i = ascii_start; i < ascii_stop; i++) {
		if(FT_Load_Char(m_face, i, FT_LOAD_RENDER)) {
			fprintf(stderr, "failed to load char '%c' aka %d\n", i, i);
			continue;
		}

		GSVector4i r(x, 0, x+c_info[i].bw, c_info[i].bh);
		if (r.width())
			t->Update(r, m_face->glyph->bitmap.buffer, m_face->glyph->bitmap.pitch);

		c_info[i].tx = (float)x / atlas_w;
		c_info[i].ty = (float)c_info[i].bh / atlas_h;
		c_info[i].tw = (float)c_info[i].bw / atlas_w;

		x += c_info[i].bw;
	}
}

void GSOsdManager::Log(std::string msg) {
	m_log.push_back((log_info){0xFF00FFFF, msg});
}

uint32 GSOsdManager::Size() {
	uint32 sum = 0;
	for(unsigned entry = 0; entry < m_log.size(); ++entry) {
		sum += m_log[entry].msg.length();
	}
	return sum * 6;
}

void GSOsdManager::GeneratePrimitives(GSVertexPT1 *dst, float m_sx, float m_sy) {
	for(unsigned entry = 0; entry < m_log.size(); ++entry) {
		float x = -1 + 8 * m_sx;
		float y = 1 - (m_size+2) * m_sy;
		for(const unsigned char* p = (unsigned char*)m_log[entry].msg.c_str(); *p; p++) {
			float x2 = x + c_info[*p].bl * m_sx;
			float y2 = -y - c_info[*p].bt * m_sy;
			float w = c_info[*p].bw * m_sx;
			float h = c_info[*p].bh * m_sy;

			/* Advance the cursor to the start of the next character */
			x += c_info[*p].ax * m_sx;
			y += c_info[*p].ay * m_sy;

			dst->p = GSVector4(x2    , -y2    , 0.0f, 0.0f);
			dst->t = GSVector2(c_info[*p].tx                , 0.0f         );
			dst->c = m_log[entry].color;
			++dst;
			dst->p = GSVector4(x2 + w, -y2    , 0.0f, 0.0f);
			dst->t = GSVector2(c_info[*p].tx + c_info[*p].tw, 0.0f         );
			dst->c = m_log[entry].color;
			++dst;
			dst->p = GSVector4(x2    , -y2 - h, 0.0f, 0.0f);
			dst->t = GSVector2(c_info[*p].tx                , c_info[*p].ty);
			dst->c = m_log[entry].color;
			++dst;
			dst->p = GSVector4(x2 + w, -y2    , 0.0f, 0.0f);
			dst->t = GSVector2(c_info[*p].tx + c_info[*p].tw, 0.0f         );
			dst->c = m_log[entry].color;
			++dst;
			dst->p = GSVector4(x2    , -y2 - h, 0.0f, 0.0f);
			dst->t = GSVector2(c_info[*p].tx                , c_info[*p].ty);
			dst->c = m_log[entry].color;
			++dst;
			dst->p = GSVector4(x2 + w, -y2 - h, 0.0f, 0.0f);
			dst->t = GSVector2(c_info[*p].tx + c_info[*p].tw, c_info[*p].ty);
			dst->c = m_log[entry].color;
			++dst;
		}
		//TODO drop line
	}
}

