int main(int argc, char **argv) {
  int i = 0;
  __sync_add_and_fetch(&i, 1);
  __sync_bool_compare_and_swap(&i, 1, 0);
  return 0;
}
