/*
 * Copyright © 2013 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Alexandros Frantzis <alexandros.frantzis@canonical.com>
 */

#ifndef MIR_GRAPHICS_DISPLAY_BUFFER_H_
#define MIR_GRAPHICS_DISPLAY_BUFFER_H_

#include <mir/geometry/rectangle.h>
#include <mir/graphics/renderable.h>
#include <mir_toolkit/common.h>

#include <memory>

namespace mir
{
namespace graphics
{

class Buffer;

/**
 * Interface to an output framebuffer.
 */
class DisplayBuffer
{
public:
    virtual ~DisplayBuffer() {}

    /** The area the DisplayBuffer occupies in the virtual screen space. */
    virtual geometry::Rectangle view_area() const = 0;
    /** Makes the DisplayBuffer the current GL rendering target. */
    virtual void make_current() = 0;
    /** Releases the current GL rendering target. */
    virtual void release_current() = 0;

    /**
     * Swap buffers for OpenGL rendering.
     * After this method returns is the earliest time that it is safe to
     * free GL-related resources such as textures and buffers.
     */
    virtual void gl_swap_buffers() = 0;

    /**
     * After gl_swap_buffers, flip the new front buffer to the screen
     * This most likely involves a wait for vblank so can be very time
     * consuming. This function is separate to gl_swap_buffers() because in
     * real display systems the act of scanning out (or flipping) the
     * front buffer is a very separate step to the GL buffer swapping. Not
     * least because "flipping" is a hardware operation that is independent
     * of the graphics library (OpenGL or other). Also, flip() can be a
     * dramatically slower operation than gl_swap_buffers() and it would be
     * an unacceptable performance hit to wait for both before freeing
     * GL resources.
     */
    virtual void flip() = 0;

    /**
     * \deprecated Please try to implement separate gl_swap_buffers and
     * flip functions instead. If not possible, just move your old
     * post_update() logic into gl_swap_buffers.
     */
    __attribute__((__deprecated__("Use gl_swap_buffers() and flip(), remembering to release all compositor buffers in the middle.")))
    void post_update() { gl_swap_buffers(); flip(); }

    /** This will render renderlist to the screen and post the result to the 
     *  screen if there is a hardware optimization that can be done.
     *  \param [in] renderlist 
     *      The renderables that should appear on the screen if the hardware
     *      is capable of optmizing that list somehow. If what you want
     *      displayed on the screen cannot be represented by a RenderableList,
     *      then you should draw using OpenGL and use post_update()
     *  \returns
     *      true if the hardware can optimize the rendering of the list.
     *      When this call completes, the renderlist will have been posted
     *      to the screen.
     *      false if the hardware platform cannot optimize the list. The screen
     *      will not be updated. The caller should render the list another way,
     *      and post using post_update()
    **/
    virtual bool post_renderables_if_optimizable(RenderableList const& renderlist) = 0;

    /** Returns the orientation of the display buffer relative to how the
     *  user should see it (the orientation of the output).
     *  This tells us how much (if any) rotation the renderer needs to do.
     *  If your DisplayBuffer can do the rotation itself then this will
     *  always return mir_orientation_normal. If the DisplayBuffer does not
     *  implement the rotation itself then this function will return the
     *  amount of rotation the renderer must do to make things "look right".
     */
    virtual MirOrientation orientation() const = 0;

    /** Returns true if the display buffer has an alpha channel and the alpha 
     *  channel will be read from at some point - in which case the renderer
     *  must produce valid alpha channel content
     */
    virtual bool uses_alpha() const = 0;

protected:
    DisplayBuffer() = default;
    DisplayBuffer(DisplayBuffer const& c) = delete;
    DisplayBuffer& operator=(DisplayBuffer const& c) = delete;
};

}
}

#endif /* MIR_GRAPHICS_DISPLAY_BUFFER_H_ */
