#include <boost/enable_shared_from_this.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/function.hpp>
#include <boost/bind.hpp>

struct Base0 : public boost::function1<void, boost::function0<void> > {
  virtual void Run() = 0;
};

template <typename A>
struct Base1 : public Base0, public boost::enable_shared_from_this<Base1<A> > {
  void Run() {
    printf ("run base1");
  }
};

template <typename C>
struct Base2 : public Base1<C>, public boost::enable_shared_from_this<Base2<C> > {
  void Run() {
    printf("run base2");
  }
};

struct Base3 : public boost::enable_shared_from_this<Base3> {
  void Run() {
    printf("run base3");
  }
};

int main(int argc, char **argv) {
  /*
  boost::shared_ptr<Base2<int> > b2(new Base2<int>);
  b2->Base1<int>::Run();
  b2->Base2<int>::shared_from_this()->Run();
  b2->Base1<int>::shared_from_this()->Run();
  boost::shared_ptr<boost::function0<void> > b1(new boost::function0<void>(boost::bind(
      &Base2<int>::Run, b2->Base1<int>::shared_from_this())));
  b2->operator()();
  */
  boost::shared_ptr<Base3> b3(new Base3);
  b3->shared_from_this()->Run();
}
