# utf8-range is bundled inside protobuf's source tree.
# protobuf builds and installs utf8_range itself (with utf8_range_ENABLE_INSTALL=ON by default).
# This port is a no-op shim so vcpkg's dependency graph is satisfied without
# installing any files that would conflict with protobuf's install tree.
set(VCPKG_BUILD_TYPE release)
set(VCPKG_POLICY_EMPTY_PACKAGE enabled)
