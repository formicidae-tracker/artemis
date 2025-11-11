#include <gmock/gmock.h>
#include <slog++/Level.hpp>
#include <slog++/slog++.hpp>

int main(int argc, char **argv) {

	::testing::InitGoogleMock(&argc, argv);

	slog::DefaultLogger().From(slog::Level::Debug);

	return RUN_ALL_TESTS();
}
