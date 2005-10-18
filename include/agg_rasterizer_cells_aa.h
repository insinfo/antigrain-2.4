//----------------------------------------------------------------------------
// Anti-Grain Geometry - Version 2.4
// Copyright (C) 2002-2005 Maxim Shemanarev (http://www.antigrain.com)
//
// Permission to copy, use, modify, sell and distribute this software 
// is granted provided this copyright notice appears in all copies. 
// This software is provided "as is" without express or implied
// warranty, and with no claim as to its suitability for any purpose.
//
//----------------------------------------------------------------------------
//
// The author gratefully acknowleges the support of David Turner, 
// Robert Wilhelm, and Werner Lemberg - the authors of the FreeType 
// libray - in producing this work. See http://www.freetype.org for details.
//
//----------------------------------------------------------------------------
// Contact: mcseem@antigrain.com
//          mcseemagg@yahoo.com
//          http://www.antigrain.com
//----------------------------------------------------------------------------
//
// Adaptation for 32-bit screen coordinates has been sponsored by 
// Liberty Technology Systems, Inc., visit http://lib-sys.com
//
// Liberty Technology Systems, Inc. is the provider of
// PostScript and PDF technology for software developers.
// 
//----------------------------------------------------------------------------
#ifndef AGG_RASTERIZER_CELLS_AA_INCLUDED
#define AGG_RASTERIZER_CELLS_AA_INCLUDED

#include <string.h>
#include <math.h>
#include "agg_math.h"
#include "agg_array.h"


namespace agg
{
    //------------------------------------------------------------------------
    // These constants determine the subpixel accuracy, to be more precise, 
    // the number of bits of the fractional part of the coordinates. 
    // The possible coordinate capacity in bits can be calculated by formula:
    // sizeof(int) * 8 - poly_base_shift * 2, i.e, for 32-bit integers and
    // 8-bits fractional part the capacity is 16 bits or [-32768...32767].
    enum poly_base_scale_e
    {
        poly_base_shift = 8,                       //----poly_base_shift
        poly_base_size  = 1 << poly_base_shift,    //----poly_base_size 
        poly_base_mask  = poly_base_size - 1       //----poly_base_mask 
    };
    
    //--------------------------------------------------------------poly_coord
    inline int poly_coord(double c)
    {
        return int(c * poly_base_size);
    }

    //-----------------------------------------------------rasterizer_cells_aa
    // An internal class that implements the main rasterization algorithm.
    // Used in the rasterizer. Should not be used direcly.
    template<class Cell> class rasterizer_cells_aa
    {
        enum cell_block_scale_e
        {
            cell_block_shift = 12,
            cell_block_size  = 1 << cell_block_shift,
            cell_block_mask  = cell_block_size - 1,
            cell_block_pool  = 256,
            cell_block_limit = 1024
        };

        struct sorted_y
        {
            unsigned start;
            unsigned num;
        };

    public:
        typedef Cell cell_type;
        typedef rasterizer_cells_aa<Cell> self_type;

        ~rasterizer_cells_aa();
        rasterizer_cells_aa();

        void reset();

        void seed_cell(const cell_type& seed);
        void move_to(int x, int y);
        void line_to(int x, int y);

        int min_x() const { return m_min_x; }
        int min_y() const { return m_min_y; }
        int max_x() const { return m_max_x; }
        int max_y() const { return m_max_y; }

        void sort_cells();

        unsigned total_cells() const 
        {
            return m_num_cells;
        }

        unsigned scanline_num_cells(unsigned y) const 
        { 
            return m_sorted_y[y - m_min_y].num; 
        }

        const cell_type* const* scanline_cells(unsigned y) const
        { 
            return m_sorted_cells.data() + m_sorted_y[y - m_min_y].start; 
        }

        bool sorted() const { return m_sorted; }

    private:
        rasterizer_cells_aa(const self_type&);
        const self_type& operator = (const self_type&);

        void set_cur_cell(int x, int y);
        void add_cur_cell();
        void render_hline(int ey, int x1, int y1, int x2, int y2);
        void render_line(int x1, int y1, int x2, int y2);
        void allocate_block();
        
    private:
        unsigned    m_num_blocks;
        unsigned    m_max_blocks;
        unsigned    m_cur_block;
        unsigned    m_num_cells;
        cell_type** m_cells;
        cell_type*  m_cur_cell_ptr;
        pod_vector<cell_type*> m_sorted_cells;
        pod_vector<sorted_y>   m_sorted_y;
        cell_type   m_cur_cell;
        cell_type   m_seed_cell;
        int         m_cur_x;
        int         m_cur_y;
        int         m_min_x;
        int         m_min_y;
        int         m_max_x;
        int         m_max_y;
        bool        m_sorted;
    };




    //------------------------------------------------------------------------
    template<class Cell> 
    rasterizer_cells_aa<Cell>::~rasterizer_cells_aa()
    {
        if(m_num_blocks)
        {
            cell_type** ptr = m_cells + m_num_blocks - 1;
            while(m_num_blocks--)
            {
                delete [] *ptr;
                ptr--;
            }
            delete [] m_cells;
        }
    }

    //------------------------------------------------------------------------
    template<class Cell> 
    rasterizer_cells_aa<Cell>::rasterizer_cells_aa() :
        m_num_blocks(0),
        m_max_blocks(0),
        m_cur_block(0),
        m_num_cells(0),
        m_cells(0),
        m_cur_cell_ptr(0),
        m_sorted_cells(),
        m_sorted_y(),
        m_cur_x(0),
        m_cur_y(0),
        m_min_x(0x7FFFFFFF),
        m_min_y(0x7FFFFFFF),
        m_max_x(-0x7FFFFFFF),
        m_max_y(-0x7FFFFFFF),
        m_sorted(false)
    {
        m_seed_cell.initial();
        m_cur_cell = m_seed_cell;
    }

    //------------------------------------------------------------------------
    template<class Cell> 
    void rasterizer_cells_aa<Cell>::reset()
    {
        m_num_cells = 0; 
        m_cur_block = 0;
        m_cur_cell = m_seed_cell;
        m_sorted = false;
        m_min_x =  0x7FFFFFFF;
        m_min_y =  0x7FFFFFFF;
        m_max_x = -0x7FFFFFFF;
        m_max_y = -0x7FFFFFFF;
    }

    //------------------------------------------------------------------------
    template<class Cell> 
    void rasterizer_cells_aa<Cell>::seed_cell(const cell_type& seed)
    {
        m_seed_cell = seed;
    }

    //------------------------------------------------------------------------
    template<class Cell> 
    void rasterizer_cells_aa<Cell>::move_to(int x, int y)
    {
        if(m_sorted) reset();

        int ex = x >> poly_base_shift;
        int ey = y >> poly_base_shift;
        if(m_cur_cell.x != ex || 
           m_cur_cell.y != ey || 
           m_cur_cell != m_seed_cell)
        {
            add_cur_cell();
            m_cur_cell = m_seed_cell;
            m_cur_cell.x = ex;
            m_cur_cell.y = ey;
            m_cur_cell.cover = 0;
            m_cur_cell.area  = 0;
        }
        m_cur_x = x;
        m_cur_y = y;
    }

    //------------------------------------------------------------------------
    template<class Cell> 
    void rasterizer_cells_aa<Cell>::line_to(int x, int y)
    {
        render_line(m_cur_x, m_cur_y, x, y);
        m_cur_x = x;
        m_cur_y = y;
        m_sorted = false;
    }

    //------------------------------------------------------------------------
    template<class Cell> 
    AGG_INLINE void rasterizer_cells_aa<Cell>::set_cur_cell(int x, int y)
    {
        if(m_cur_cell.x != x || m_cur_cell.y != y)
        {
            add_cur_cell();
            m_cur_cell.x = x;
            m_cur_cell.y = y;
            m_cur_cell.cover = 0;
            m_cur_cell.area  = 0;
        }
    }

    //------------------------------------------------------------------------
    template<class Cell> 
    AGG_INLINE void rasterizer_cells_aa<Cell>::add_cur_cell()
    {
        if(m_cur_cell.area | m_cur_cell.cover)
        {
            if((m_num_cells & cell_block_mask) == 0)
            {
                if(m_num_blocks >= cell_block_limit) return;
                allocate_block();
            }
            *m_cur_cell_ptr++ = m_cur_cell;
            ++m_num_cells;
            if(m_cur_cell.x < m_min_x) m_min_x = m_cur_cell.x;
            if(m_cur_cell.x > m_max_x) m_max_x = m_cur_cell.x;
            if(m_cur_cell.y < m_min_y) m_min_y = m_cur_cell.y;
            if(m_cur_cell.y > m_max_y) m_max_y = m_cur_cell.y;
        }
    }

    //------------------------------------------------------------------------
    template<class Cell> 
    AGG_INLINE void rasterizer_cells_aa<Cell>::render_hline(int ey, 
                                                            int x1, int y1, 
                                                            int x2, int y2)
    {
        int ex1 = x1 >> poly_base_shift;
        int ex2 = x2 >> poly_base_shift;
        int fx1 = x1 & poly_base_mask;
        int fx2 = x2 & poly_base_mask;

        int delta, p, first, dx;
        int incr, lift, mod, rem;

        //trivial case. Happens often
        if(y1 == y2)
        {
            set_cur_cell(ex2, ey);
            return;
        }

        //everything is located in a single cell.  That is easy!
        if(ex1 == ex2)
        {
            delta = y2 - y1;
            m_cur_cell.cover += delta;
            m_cur_cell.area  += (fx1 + fx2) * delta;
            return;
        }

        //ok, we'll have to render a run of adjacent cells on the same
        //hline...
        p     = (poly_base_size - fx1) * (y2 - y1);
        first = poly_base_size;
        incr  = 1;

        dx = x2 - x1;

        if(dx < 0)
        {
            p     = fx1 * (y2 - y1);
            first = 0;
            incr  = -1;
            dx    = -dx;
        }

        delta = p / dx;
        mod   = p % dx;

        if(mod < 0)
        {
            delta--;
            mod += dx;
        }

        m_cur_cell.cover += delta;
        m_cur_cell.area  += (fx1 + first) * delta;

        ex1 += incr;
        set_cur_cell(ex1, ey);
        y1  += delta;

        if(ex1 != ex2)
        {
            p     = poly_base_size * (y2 - y1 + delta);
            lift  = p / dx;
            rem   = p % dx;

            if (rem < 0)
            {
                lift--;
                rem += dx;
            }

            mod -= dx;

            while (ex1 != ex2)
            {
                delta = lift;
                mod  += rem;
                if(mod >= 0)
                {
                    mod -= dx;
                    delta++;
                }

                m_cur_cell.cover += delta;
                m_cur_cell.area  += poly_base_size * delta;
                y1  += delta;
                ex1 += incr;
                set_cur_cell(ex1, ey);
            }
        }
        delta = y2 - y1;
        m_cur_cell.cover += delta;
        m_cur_cell.area  += (fx2 + poly_base_size - first) * delta;
    }

    //------------------------------------------------------------------------
    template<class Cell> 
    void rasterizer_cells_aa<Cell>::render_line(int x1, int y1, int x2, int y2)
    {
        enum dx_limit_e { dx_limit = 16384 << poly_base_shift };

        int dx = x2 - x1;

        if(dx >= dx_limit || dx <= -dx_limit)
        {
            int cx = (x1 + x2) >> 1;
            int cy = (y1 + y2) >> 1;
            render_line(x1, y1, cx, cy);
            render_line(cx, cy, x2, y2);
        }

        int dy = y2 - y1;
        int ey1 = y1 >> poly_base_shift;
        int ey2 = y2 >> poly_base_shift;
        int fy1 = y1 & poly_base_mask;
        int fy2 = y2 & poly_base_mask;

        int x_from, x_to;
        int p, rem, mod, lift, delta, first, incr;

        //everything is on a single hline
        if(ey1 == ey2)
        {
            render_hline(ey1, x1, fy1, x2, fy2);
            return;
        }

        //Vertical line - we have to calculate start and end cells,
        //and then - the common values of the area and coverage for
        //all cells of the line. We know exactly there's only one 
        //cell, so, we don't have to call render_hline().
        incr  = 1;
        if(dx == 0)
        {
            int ex = x1 >> poly_base_shift;
            int two_fx = (x1 - (ex << poly_base_shift)) << 1;
            int area;

            first = poly_base_size;
            if(dy < 0)
            {
                first = 0;
                incr  = -1;
            }

            x_from = x1;

            //render_hline(ey1, x_from, fy1, x_from, first);
            delta = first - fy1;
            m_cur_cell.cover += delta;
            m_cur_cell.area  += two_fx * delta;

            ey1 += incr;
            set_cur_cell(ex, ey1);

            delta = first + first - poly_base_size;
            area = two_fx * delta;
            while(ey1 != ey2)
            {
                //render_hline(ey1, x_from, poly_base_size - first, x_from, first);
                m_cur_cell.cover = delta;
                m_cur_cell.area  = area;
                ey1 += incr;
                set_cur_cell(ex, ey1);
            }
            //render_hline(ey1, x_from, poly_base_size - first, x_from, fy2);
            delta = fy2 - poly_base_size + first;
            m_cur_cell.cover += delta;
            m_cur_cell.area  += two_fx * delta;
            return;
        }

        //ok, we have to render several hlines
        p     = (poly_base_size - fy1) * dx;
        first = poly_base_size;

        if(dy < 0)
        {
            p     = fy1 * dx;
            first = 0;
            incr  = -1;
            dy    = -dy;
        }

        delta = p / dy;
        mod   = p % dy;

        if(mod < 0)
        {
            delta--;
            mod += dy;
        }

        x_from = x1 + delta;
        render_hline(ey1, x1, fy1, x_from, first);

        ey1 += incr;
        set_cur_cell(x_from >> poly_base_shift, ey1);

        if(ey1 != ey2)
        {
            p     = poly_base_size * dx;
            lift  = p / dy;
            rem   = p % dy;

            if(rem < 0)
            {
                lift--;
                rem += dy;
            }
            mod -= dy;

            while(ey1 != ey2)
            {
                delta = lift;
                mod  += rem;
                if (mod >= 0)
                {
                    mod -= dy;
                    delta++;
                }

                x_to = x_from + delta;
                render_hline(ey1, x_from, poly_base_size - first, x_to, first);
                x_from = x_to;

                ey1 += incr;
                set_cur_cell(x_from >> poly_base_shift, ey1);
            }
        }
        render_hline(ey1, x_from, poly_base_size - first, x2, fy2);
    }

    //------------------------------------------------------------------------
    template<class Cell> 
    void rasterizer_cells_aa<Cell>::allocate_block()
    {
        if(m_cur_block >= m_num_blocks)
        {
            if(m_num_blocks >= m_max_blocks)
            {
                cell_type** new_cells = new cell_type* [m_max_blocks + cell_block_pool];
                if(m_cells)
                {
                    memcpy(new_cells, m_cells, m_max_blocks * sizeof(cell_type*));
                    delete [] m_cells;
                }
                m_cells = new_cells;
                m_max_blocks += cell_block_pool;
            }
            m_cells[m_num_blocks++] = new cell_type [unsigned(cell_block_size)];
        }
        m_cur_cell_ptr = m_cells[m_cur_block++];
    }



    //------------------------------------------------------------------------
    template <class T> static AGG_INLINE void swap_cells(T* a, T* b)
    {
        T temp = *a;
        *a = *b;
        *b = temp;
    }


    //------------------------------------------------------------------------
    enum
    {
        qsort_threshold = 9
    };


    //------------------------------------------------------------------------
    template<class Cell>
    void qsort_cells(Cell** start, unsigned num)
    {
        Cell**  stack[80];
        Cell*** top; 
        Cell**  limit;
        Cell**  base;

        limit = start + num;
        base  = start;
        top   = stack;

        for (;;)
        {
            int len = int(limit - base);

            Cell** i;
            Cell** j;
            Cell** pivot;

            if(len > qsort_threshold)
            {
                // we use base + len/2 as the pivot
                pivot = base + len / 2;
                swap_cells(base, pivot);

                i = base + 1;
                j = limit - 1;

                // now ensure that *i <= *base <= *j 
                if((*j)->x < (*i)->x)
                {
                    swap_cells(i, j);
                }

                if((*base)->x < (*i)->x)
                {
                    swap_cells(base, i);
                }

                if((*j)->x < (*base)->x)
                {
                    swap_cells(base, j);
                }

                for(;;)
                {
                    int x = (*base)->x;
                    do i++; while( (*i)->x < x );
                    do j--; while( x < (*j)->x );

                    if(i > j)
                    {
                        break;
                    }

                    swap_cells(i, j);
                }

                swap_cells(base, j);

                // now, push the largest sub-array
                if(j - base > limit - i)
                {
                    top[0] = base;
                    top[1] = j;
                    base   = i;
                }
                else
                {
                    top[0] = i;
                    top[1] = limit;
                    limit  = j;
                }
                top += 2;
            }
            else
            {
                // the sub-array is small, perform insertion sort
                j = base;
                i = j + 1;

                for(; i < limit; j = i, i++)
                {
                    for(; j[1]->x < (*j)->x; j--)
                    {
                        swap_cells(j + 1, j);
                        if (j == base)
                        {
                            break;
                        }
                    }
                }

                if(top > stack)
                {
                    top  -= 2;
                    base  = top[0];
                    limit = top[1];
                }
                else
                {
                    break;
                }
            }
        }
    }


    //------------------------------------------------------------------------
    template<class Cell> 
    void rasterizer_cells_aa<Cell>::sort_cells()
    {
        if(m_sorted) return; //Perform sort only the first time.

        add_cur_cell();

        if(m_num_cells == 0) return;

        // Allocate the array of cell pointers
        m_sorted_cells.allocate(m_num_cells, 16);

        // Allocate and zero the Y array
        m_sorted_y.allocate(m_max_y - m_min_y + 1, 16);
        m_sorted_y.zero();

        // Create the Y-histogram (count the numbers of cells for each Y)
        cell_type** block_ptr = m_cells;
        cell_type*  cell_ptr;
        unsigned nb = m_num_cells >> cell_block_shift;
        unsigned i;
        while(nb--)
        {
            cell_ptr = *block_ptr++;
            i = cell_block_size;
            while(i--) 
            {
                m_sorted_y[cell_ptr->y - m_min_y].start++;
                ++cell_ptr;
            }
        }

        cell_ptr = *block_ptr++;
        i = m_num_cells & cell_block_mask;
        while(i--) 
        {
            m_sorted_y[cell_ptr->y - m_min_y].start++;
            ++cell_ptr;
        }

        // Convert the Y-histogram into the array of starting indexes
        unsigned start = 0;
        for(i = 0; i < m_sorted_y.size(); i++)
        {
            unsigned v = m_sorted_y[i].start;
            m_sorted_y[i].start = start;
            start += v;
        }

        // Fill the cell pointer array sorted by Y
        block_ptr = m_cells;
        nb = m_num_cells >> cell_block_shift;
        while(nb--)
        {
            cell_ptr = *block_ptr++;
            i = cell_block_size;
            while(i--) 
            {
                sorted_y& cur_y = m_sorted_y[cell_ptr->y - m_min_y];
                m_sorted_cells[cur_y.start + cur_y.num] = cell_ptr;
                ++cur_y.num;
                ++cell_ptr;
            }
        }
        
        cell_ptr = *block_ptr++;
        i = m_num_cells & cell_block_mask;
        while(i--) 
        {
            sorted_y& cur_y = m_sorted_y[cell_ptr->y - m_min_y];
            m_sorted_cells[cur_y.start + cur_y.num] = cell_ptr;
            ++cur_y.num;
            ++cell_ptr;
        }

        // Finally arrange the X-arrays
        for(i = 0; i < m_sorted_y.size(); i++)
        {
            const sorted_y& cur_y = m_sorted_y[i];
            if(cur_y.num)
            {
                qsort_cells(m_sorted_cells.data() + cur_y.start, cur_y.num);
            }
        }
        m_sorted = true;
    }

}

#endif