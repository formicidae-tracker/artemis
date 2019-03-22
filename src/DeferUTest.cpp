#include "DeferUTest.h"
#include "Defer.h"



void doSomething(bool returnEarly, bool & flag) {
	auto defer = Defer([&flag](){
			flag = true;
		});

	if ( returnEarly == true ) {
		return;
	}
}


TEST(DeferUTest,TestItCanDefer) {
	bool flag = false;

	doSomething(false,flag);

	EXPECT_EQ(flag, true);
	flag = false;

	doSomething(true,flag);
	EXPECT_EQ(flag, true);

}
