KEYUTILS_0.3 {

	/* primary syscalls; may be overridden by glibc */
	add_key;
	request_key;
	keyctl;

	/* management functions */
	keyctl_chown;
	keyctl_clear;
	keyctl_describe;
	keyctl_describe_alloc;
	keyctl_get_keyring_ID;
	keyctl_instantiate;
	keyctl_join_session_keyring;
	keyctl_link;
	keyctl_negate;
	keyctl_read;
	keyctl_read_alloc;
	keyctl_revoke;
	keyctl_search;
	keyctl_setperm;
	keyctl_set_reqkey_keyring;
	keyctl_unlink;
	keyctl_update;
	 
};

KEYUTILS_1.0 {
	/* management functions */
	keyctl_assume_authority;
	keyctl_set_timeout;

} KEYUTILS_0.3;

KEYUTILS_1.3 {
	/* management functions */
	keyctl_get_security;
	keyctl_get_security_alloc;
	keyctl_session_to_parent;

} KEYUTILS_1.0;
