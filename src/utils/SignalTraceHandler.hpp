#pragma once

namespace fort {
namespace artemis {
void InstallSignalSafeHandlers(int arg, char **argv, bool sigtrap = false);
}
} // namespace fort
