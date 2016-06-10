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
#include <algorithm>

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

		c_info[i].tx = atlas_w;

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

  m_log_len = 1; // number of log lines to initialize space for
  m_log_count = 0;
  m_log = (log_info*)_aligned_malloc(sizeof(log_info)*m_log_len, 16);

  m_grp_len = 1; // number of groups to initialize space for
  NumberOfGroups = 0;
  Color = (GSVector4*)_aligned_malloc(sizeof(GSVector4)*m_grp_len, 16);

  m_elem_len = 25; // number of characters to initialize space for
  NumberOfElements.push_back(0);
  Vertex = (GSVector4*)_aligned_malloc(sizeof(GSVector4)*m_elem_len*6, 16);
  Texcoord = (GSVector2*)_aligned_malloc(sizeof(GSVector2)*m_elem_len*6, 16);
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

    x += c_info[i].bw;
	}
}

unsigned GSOsdManager::ExpandAligned(void **mem, size_t block, unsigned len) {
  //std::vector doesn't honor alignas so do it old school
  void *temp = *mem;
  *mem = _aligned_malloc(block*len*2, 16);
  memcpy(*mem, temp, block*len);
  _aligned_free(temp);
  return len * 2;
}

void GSOsdManager::Log(std::string msg) {
  if(m_log_count == m_log_len)
    m_log_len = ExpandAligned((void **)&m_log, sizeof(*m_log), m_log_len);

  new(m_log+m_log_count) log_info();
  m_log[m_log_count].color = GSVector4(1, 1, 0, 1);
  m_log[m_log_count].msg = msg;
  ++m_log_count;
}

void GSOsdManager::GeneratePrimitives(float m_sx, float m_sy) {
  NumberOfGroups = 0;
  float x = -1 + 8 * m_sx;
  float y = 1 - (m_size+2) * m_sy;

  for(unsigned entry = 0; entry < m_log_count; ++entry) {
    if(NumberOfGroups >= m_grp_len) {
      m_grp_len = ExpandAligned((void **)&Color, sizeof(*Color), m_grp_len);
      NumberOfElements.push_back(0);
    }

    Color[NumberOfGroups] = m_log[entry].color;
    NumberOfElements[NumberOfGroups] = 0;

    for(const unsigned char* p = (unsigned char*)m_log[entry].msg.c_str(); *p; p++) {
      float x2 = x + c_info[*p].bl * m_sx;
      float y2 = -y - c_info[*p].bt * m_sy;
      float w = c_info[*p].bw * m_sx;
      float h = c_info[*p].bh * m_sy;

      /* Advance the cursor to the start of the next character */
      x += c_info[*p].ax * m_sx;
      y += c_info[*p].ay * m_sy;

      // NOTE: In the future we could use only a SHORT for texture
      // coordinate. And do the division by the texture size on the vertex
      // shader (need to drop dx9 compatibility)
      m_WorkVector4[0] = GSVector4(x2    , -y2    , 0.0f, 0.0f);
      m_WorkVector4[1] = GSVector4(x2 + w, -y2    , 0.0f, 0.0f);
      m_WorkVector4[2] = GSVector4(x2    , -y2 - h, 0.0f, 0.0f);
      m_WorkVector4[3] = GSVector4(x2 + w, -y2    , 0.0f, 0.0f);
      m_WorkVector4[4] = GSVector4(x2    , -y2 - h, 0.0f, 0.0f);
      m_WorkVector4[5] = GSVector4(x2 + w, -y2 - h, 0.0f, 0.0f);

      m_WorkVector2[0] = GSVector2(c_info[*p].tx                          , 0.0f         );
      m_WorkVector2[1] = GSVector2(c_info[*p].tx + ((float)c_info[*p].bw / atlas_w), 0.0f         );
      m_WorkVector2[2] = GSVector2(c_info[*p].tx                          , c_info[*p].ty);
      m_WorkVector2[3] = GSVector2(c_info[*p].tx + ((float)c_info[*p].bw / atlas_w), 0.0f         );
      m_WorkVector2[4] = GSVector2(c_info[*p].tx                          , c_info[*p].ty);
      m_WorkVector2[5] = GSVector2(c_info[*p].tx + ((float)c_info[*p].bw / atlas_w), c_info[*p].ty);

      if(NumberOfElements[NumberOfGroups]+6 >= m_elem_len) {
        ExpandAligned((void **)&Vertex, sizeof(*Vertex), m_elem_len);
        m_elem_len = ExpandAligned((void **)&Texcoord, sizeof(*Texcoord), m_elem_len);
      }
      memcpy(&Vertex[NumberOfElements[NumberOfGroups]], m_WorkVector4, 6*sizeof(GSVector4));
      memcpy(&Texcoord[NumberOfElements[NumberOfGroups]], m_WorkVector2, 6*sizeof(GSVector2));

      NumberOfElements[NumberOfGroups] += 6;
    }

    ++NumberOfGroups;
  }
}

