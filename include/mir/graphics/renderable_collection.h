/*
 * Copyright © 2012 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored by: Robert Carr <robert.carr@canonical.com>
 */

#ifndef MIR_GRAPHICS_RENDERABLE_COLLECTION_H
#define MIR_GRAPHICS_RENDERABLE_COLLECTION_H

#include <memory>

namespace mir
{
namespace graphics
{

class Renderable;

class RenderableEnumerator
{
public:
    virtual ~RenderableEnumerator() {}

    virtual void operator()(Renderable& renderable) = 0;

protected:
    RenderableEnumerator() = default;
    RenderableEnumerator(const RenderableEnumerator&) = delete;
    RenderableEnumerator& operator=(const RenderableEnumerator&) = delete;
};

class RenderableCollection
{
public:
    virtual ~RenderableCollection() {}

    virtual void invoke_for_each_renderable(RenderableEnumerator& enumerator) = 0;

  protected:    
    RenderableCollection() = default;
    RenderableCollection(const RenderableCollection&) = delete;
    RenderableCollection& operator=(const RenderableCollection&) = delete;

};
}
}

#endif /* MIR_GRAPHICS_RENDERABLE_COLLECTION_H */
