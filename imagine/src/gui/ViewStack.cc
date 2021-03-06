/*  This file is part of Imagine.

	Imagine is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	Imagine is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with Imagine.  If not, see <http://www.gnu.org/licenses/> */

#include <gui/ViewStack.hh>
#include <utility>

void BasicViewController::push(View &v)
{
	if(view)
	{
		logMsg("removing existing view from basic view controller");
		pop();
	}
	v.controller = this;
	view = &v;
	logMsg("push view in basic view controller");
}

void BasicViewController::pushAndShow(View &v, StackAllocator *allocator, bool needsNavView)
{
	push(v);
	place();
	v.show();
	v.postDraw();
}

void BasicViewController::pushAndShow(View &v)
{
	pushAndShow(v, nullptr, true);
}

void BasicViewController::pop()
{
	assert(view);
	view->postDraw();
	view->deinit();
	view = nullptr;
	if(removeViewDel)
		removeViewDel();
}

void BasicViewController::dismissView(View &v)
{
	assert(view == &v);
	pop();
}

void BasicViewController::place(const IG::WindowRect &rect)
{
	viewRect = rect;
	place();
}

void BasicViewController::place()
{
	if(!view)
		return;
	view->placeRect(viewRect);
}

void BasicViewController::inputEvent(const Input::Event &e)
{
	view->inputEvent(e);
}

void BasicViewController::draw(Base::FrameTimeBase frameTime)
{
	view->draw(frameTime);
}

void BasicViewController::init(const Base::Window &win)
{
	viewRect = Gfx::viewport().bounds();
}

void ViewStack::init(const Base::Window &win)
{
	viewRect = Gfx::viewport().bounds();
	if(size)
	{
		view[0]->controller = this;
	}
}

void ViewStack::setNavView(NavView *nav)
{
	var_selfs(nav);
	if(nav)
	{
		nav->setLeftBtnActive(size > 1);
		useNavView = 1;
	}
}

NavView *ViewStack::navView() const
{
	return nav;
}

void ViewStack::place(const IG::WindowRect &rect)
{
	viewRect = rect;
	place();
}

void ViewStack::place()
{
	if(!size)
		return;
	customViewRect = viewRect;
	if(useNavView && nav)
	{
		nav->setTitle(top().name());
		nav->viewRect.setPosRel({viewRect.x, viewRect.y}, {viewRect.xSize(), IG::makeEvenRoundedUp(int(nav->text.face->nominalHeight()*1.75))}, LT2DO);
		nav->place();
		customViewRect.y += nav->viewRect.ySize();
	}
	top().placeRect(customViewRect);
}

void ViewStack::inputEvent(const Input::Event &e)
{
	if(useNavView && nav && e.isPointer() && nav->viewRect.overlaps({e.x, e.y}))
	{
		nav->inputEvent(e);
	}
	top().inputEvent(e);
}

void ViewStack::draw(Base::FrameTimeBase frameTime)
{
	top().draw(frameTime);
	if(useNavView && nav) nav->draw(top().window());
}

void ViewStack::push(View &v, StackAllocator *allocator)
{
	assert(size != sizeofArray(view));
	v.controller = this;
	view[size] = &v;
	viewAllocator[size] = allocator;
	size++;
	logMsg("push view, %d in stack", size);

	if(nav)
	{
		nav->setLeftBtnActive(size > 1);
	}
}

void ViewStack::push(View &v)
{
	push(v, nullptr);
}

void ViewStack::pushAndShow(View &v, StackAllocator *allocator, bool needsNavView)
{
	useNavView = needsNavView;
	push(v, allocator);
	place();
	v.show();
	v.postDraw();
}

void ViewStack::pushAndShow(View &v, StackAllocator *allocator)
{
	pushAndShow(v, allocator, true);
}

void ViewStack::pushAndShow(View &v)
{
	pushAndShow(v, nullptr, true);
}

void ViewStack::pop()
{
	top().deinit();
	if(viewAllocator[size-1])
	{
		viewAllocator[size-1]->pop();
	}
	size--;
	logMsg("pop view, %d in stack", size);
	assert(size != 0);

	if(nav)
	{
		nav->setLeftBtnActive(size > 1);
		nav->setTitle(top().name());
		useNavView = 1;
	}
}

void ViewStack::popAndShow()
{
	pop();
	place();
	top().show();
	top().postDraw();
}

void ViewStack::popToRoot()
{
	while(size > 1)
		pop();
	place();
	top().show();
	top().postDraw();
}

void ViewStack::popTo(View &v)
{
	while(size > 1 && view[size-1] != &v)
		pop();
	place();
	top().show();
	top().postDraw();
}

void ViewStack::show()
{
	top().show();
}

View &ViewStack::top() const
{
	assert(size != 0);
	return *view[size-1];
}

View &ViewStack::viewAtIdx(uint idx) const
{
	assert(idx < size);
	return *view[idx];
}

int ViewStack::viewIdx(View &v) const
{
	iterateTimes(size, i)
	{
		if(view[i] == &v)
			return i;
	}
	return -1;
}

bool ViewStack::contains(View &v) const
{
	return viewIdx(v) != -1;
}

void ViewStack::dismissView(View &v)
{
	//logMsg("dismissing view: %p", &v);
	assert(contains(v));
	assert(viewIdx(v) != 0);
	popTo(*view[viewIdx(v)-1]);
}
