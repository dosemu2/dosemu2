
main()
{
  unsigned char a=1;
  long long int b=10;
  char c='0';
  printf ("Size of short int=%d, longint=%d , char=%d\n", sizeof(a), sizeof(b), sizeof(c) );
  printf ("0=%x\n",c); 
  a=1;
  a= a + 0xff; 
  printf ("a = %d\n", a);
  a=a + 0xff; 
  printf ("a = %d\n", a);
  a=a + 0xff; 
  printf ("a = %d\n", a);
  a=a + 0xff; 
  printf ("a = %d\n", a);
  a=a + 0xff; 
  printf ("a = %d\n", a);
}
