                            2  R   	  �    #ifndef GABA_DEFINEstruct lsh_writekey_options{  struct lsh_object super;  struct lsh_string *file;  struct interact *tty;  struct lsh_string *label;  sexp_argp_state style;  struct lsh_string *passphrase;  struct alist *crypto_algorithms;  struct alist *signature_algorithms;  struct randomness *r;  int crypto_name;  struct crypto_algorithm *crypto;  UINT32 iterations;};extern struct lsh_class lsh_writekey_options_class;#endif /* !GABA_DEFINE */#ifndef GABA_DECLAREstatic void do_lsh_writekey_options_mark(struct lsh_object *o, void (*mark)(struct lsh_object *o)){  struct lsh_writekey_options *i = (struct lsh_writekey_options *) o;  mark((struct lsh_object *) i->tty);  mark((struct lsh_object *) i->crypto_algorithms);  mark((struct lsh_object *) i->signature_algorithms);  mark((struct lsh_object *) i->r);  mark((struct lsh_object *) i->crypto);}static void do_lsh_writekey_options_free(struct lsh_object *o){  struct lsh_writekey_options *i = (struct lsh_writekey_options *) o;  lsh_string_free(i->file);  lsh_string_free(i->label);  lsh_string_free(i->passphrase);}struct lsh_class lsh_writekey_options_class ={ STATIC_HEADER,  0, "lsh_writekey_options", sizeof(struct lsh_writekey_options),  do_lsh_writekey_options_mark,  do_lsh_writekey_options_free};#endif /* !GABA_DECLARE */static struct lsh_object *make_writekey(struct lsh_writekey_options *options){  /* (S (S (B S (C (B* (B* prog1) (print_public options) (C open (options2public_file options))) (B* verifier2public signer2verifier (sexp2signer (options2algorithms options))))) (C (B* B (print_private options) (C open (options2private_file options))) (transform options))) (B read_sexp stdin)) */#define A GABA_APPLY#define I GABA_VALUE_I#define K GABA_VALUE_K#define K1 GABA_APPLY_K_1#define S GABA_VALUE_S#define S1 GABA_APPLY_S_1#define S2 GABA_APPLY_S_2#define B GABA_VALUE_B#define B1 GABA_APPLY_B_1#define B2 GABA_APPLY_B_2#define C GABA_VALUE_C#define C1 GABA_APPLY_C_1#define C2 GABA_APPLY_C_2#define Sp GABA_VALUE_Sp#define Sp1 GABA_APPLY_Sp_1#define Sp2 GABA_APPLY_Sp_2#define Sp3 GABA_APPLY_Sp_3#define Bp GABA_VALUE_Bp#define Bp1 GABA_APPLY_Bp_1#define Bp2 GABA_APPLY_Bp_2#define Bp3 GABA_APPLY_Bp_3#define Cp GABA_VALUE_Cp#define Cp1 GABA_APPLY_Cp_1#define Cp2 GABA_APPLY_Cp_2#define Cp3 GABA_APPLY_Cp_3  return MAKE_TRACE("make_writekey",     S2(S2(B2(S, C2(Bp3(Bp1(PROG1), A(PRINT_PUBLIC, ((struct lsh_object *) options)), C2(IO_WRITE_FILE, A(OPTIONS2PUBLIC_FILE, ((struct lsh_object *) options)))), Bp3(VERIFIER2PUBLIC, SIGNER2VERIFIER, A(SEXP2SIGNER, A(OPTIONS2ALGORITHMS, ((struct lsh_object *) options)))))), C2(Bp3(B, A(PRINT_PRIVATE, ((struct lsh_object *) options)), C2(IO_WRITE_FILE, A(OPTIONS2PRIVATE_FILE, ((struct lsh_object *) options)))), A(TRANSFORM, ((struct lsh_object *) options)))), B2(READ_SEXP, IO_READ_STDIN))  );#undef A#undef I#undef K#undef K1#undef S#undef S1#undef S2#undef B#undef B1#undef B2#undef C#undef C1#undef C2#undef Sp#undef Sp1#undef Sp2#undef Sp3#undef Bp#undef Bp1#undef Bp2#undef Bp3#undef Cp#undef Cp1#undef Cp2#undef Cp3}TEXTCWIE 	 �                  