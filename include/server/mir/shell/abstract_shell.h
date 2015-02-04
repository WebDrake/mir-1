/*
 * Copyright © 2015 Canonical Ltd.
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 3,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Authored By: Alan Griffiths <alan@octopull.co.uk>
 */

#ifndef MIR_SHELL_ABSTRACT_SHELL_H_
#define MIR_SHELL_ABSTRACT_SHELL_H_

#include "mir/shell/shell.h"

#include <mutex>

namespace mir
{
namespace shell
{
/// Minimal Shell implementation with none of the necessary window management logic
class AbstractShell : public virtual Shell
{
public:
    AbstractShell(
        std::shared_ptr<InputTargeter> const& input_targeter,
        std::shared_ptr<scene::SurfaceCoordinator> const& surface_coordinator,
        std::shared_ptr<scene::SessionCoordinator> const& session_coordinator,
        std::shared_ptr<scene::PromptSessionManager> const& prompt_session_manager);

    ~AbstractShell() noexcept;

    std::shared_ptr<scene::Session> open_session(
        pid_t client_pid,
        std::string const& name,
        std::shared_ptr<frontend::EventSink> const& sink) override;

    void close_session(std::shared_ptr<scene::Session> const& session) override;

    frontend::SurfaceId create_surface(std::shared_ptr<scene::Session> const& session, scene::SurfaceCreationParameters const& params) override;

    void destroy_surface(std::shared_ptr<scene::Session> const& session, frontend::SurfaceId surface) override;

    void handle_surface_created(std::shared_ptr<scene::Session> const& session) override;

    int set_surface_attribute(
        std::shared_ptr<scene::Session> const& session,
        std::shared_ptr<scene::Surface> const& surface,
        MirSurfaceAttrib attrib,
        int value) override;

    int get_surface_attribute(
        std::shared_ptr<scene::Surface> const& surface,
        MirSurfaceAttrib attrib) override;

    std::shared_ptr<scene::PromptSession> start_prompt_session_for(
        std::shared_ptr<scene::Session> const& session,
        scene::PromptSessionCreationParameters const& params) override;

    void add_prompt_provider_for(
        std::shared_ptr<scene::PromptSession> const& prompt_session,
        std::shared_ptr<scene::Session> const& session) override;

    void stop_prompt_session(std::shared_ptr<scene::PromptSession> const& prompt_session) override;

/** @name these come from FocusController
 * Focus changes are notified to the derived class via the private setting_focus_to()
 * functions.
 * \note I think the FocusController interface is unnecessary as:
 *   1. the functions are only meaningful in the context of implementing a Shell
 *   2. the implementation of these functions is Shell behaviour
 * Simply providing them as part of AbstractShell is probably adequate.
 *  @{ */
    void focus_next() override;

    std::weak_ptr<scene::Session> focussed_application() const override;

    void set_focus_to(std::shared_ptr<scene::Session> const& focus) override;
/** @} */

protected:
    std::shared_ptr<InputTargeter> const input_targeter;
    std::shared_ptr<scene::SurfaceCoordinator> const surface_coordinator;
    std::shared_ptr<scene::SessionCoordinator> const session_coordinator;
    std::shared_ptr<scene::PromptSessionManager> const prompt_session_manager;

private:
/** @name callbacks from FocusController methods
 * These functions are called while the input focus is locked.
 * \warning DO NOT allow recursive calls to focus_next(), focussed_application() or set_focus_to()
 * as these will deadlock.
 * @{ */
    virtual void setting_focus_to(std::shared_ptr<scene::Surface> const& next_focus);
    virtual void setting_focus_to(std::shared_ptr<scene::Session> const& next_focus);
/** @} */

    std::mutex mutable focus_mutex;
    std::weak_ptr<scene::Surface> focus_surface;
    std::weak_ptr<scene::Session> focus_session;

    void set_focus_to_locked(std::unique_lock<std::mutex> const& lock, std::shared_ptr<scene::Surface> const& next_focus);
    void set_focus_to_locked(std::unique_lock<std::mutex> const& lock, std::shared_ptr<scene::Session> const& next_focus);
};
}
}

#endif /* MIR_SHELL_ABSTRACT_SHELL_H_ */