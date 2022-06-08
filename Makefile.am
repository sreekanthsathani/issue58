ACLOCAL_AMFLAGS = -I m4
bin_PROGRAMS = idrivebmrsrv idrivebmr_snapshot_helper idrivebmr_mount_helper
idrivebmrsrv_SOURCES = AcceptThread.cpp Client.cpp Database.cpp Query.cpp SelectThread.cpp Server.cpp ServerLinux.cpp ServiceAcceptor.cpp ServiceWorker.cpp SessionMgr.cpp StreamPipe.cpp Template.cpp WorkerThread.cpp main.cpp md5.cpp stringtools.cpp libfastcgi/fastcgi.cpp Mutex_lin.cpp LoadbalancerClient.cpp DBSettingsReader.cpp file_common.cpp file_fstream.cpp file_linux.cpp FileSettingsReader.cpp LookupService.cpp SettingsReader.cpp Table.cpp OutputStream.cpp ThreadPool.cpp MemoryPipe.cpp Condition_lin.cpp MemorySettingsReader.cpp sqlite/sqlite3.c sqlite/shell.c SQLiteFactory.cpp PipeThrottler.cpp mt19937ar.cpp DatabaseCursor.cpp SharedMutex_lin.cpp StaticPluginRegistration.cpp common/data.cpp common/adler32.cpp common/miniz.c

idrivebmrsrv_SOURCES += fsimageplugin/dllmain.cpp fsimageplugin/filesystem.cpp fsimageplugin/FSImageFactory.cpp fsimageplugin/pluginmgr.cpp fsimageplugin/vhdfile.cpp fsimageplugin/fs/ntfs.cpp fsimageplugin/fs/unknown.cpp fsimageplugin/CompressedFile.cpp fsimageplugin/LRUMemCache.cpp fsimageplugin/cowfile.cpp fsimageplugin/FileWrapper.cpp fsimageplugin/ClientBitmap.cpp

idrivebmrsrv_SOURCES += idrivebmrcommon/os_functions_lin.cpp idrivebmrcommon/sha2/sha2.cpp idrivebmrcommon/fileclient/FileClient.cpp idrivebmrcommon/fileclient/tcpstack.cpp idrivebmrcommon/escape.cpp idrivebmrcommon/bufmgr.cpp idrivebmrcommon/json.cpp idrivebmrcommon/CompressedPipe.cpp idrivebmrcommon/InternetServicePipe2.cpp idrivebmrcommon/settingslist.cpp idrivebmrcommon/fileclient/FileClientChunked.cpp idrivebmrcommon/InternetServicePipe.cpp idrivebmrcommon/filelist_utils.cpp idrivebmrcommon/file_metadata.cpp idrivebmrcommon/glob.cpp idrivebmrcommon/chunk_hasher.cpp idrivebmrcommon/CompressedPipe2.cpp idrivebmrcommon/SparseFile.cpp idrivebmrcommon/ExtentIterator.cpp idrivebmrcommon/TreeHash.cpp

idrivebmrsrv_SOURCES += httpserver/dllmain.cpp httpserver/IndexFiles.cpp httpserver/HTTPAction.cpp httpserver/HTTPFile.cpp httpserver/HTTPService.cpp httpserver/HTTPClient.cpp httpserver/HTTPProxy.cpp httpserver/MIMEType.cpp

idrivebmrsrv_SOURCES += idrivebmrserver/dllmain.cpp idrivebmrserver/server.cpp idrivebmrserver/ClientMain.cpp idrivebmrserver/server_hash.cpp idrivebmrserver/server_prepare_hash.cpp idrivebmrserver/server_update.cpp idrivebmrserver/server_status.cpp idrivebmrserver/server_channel.cpp idrivebmrserver/server_ping.cpp idrivebmrserver/server_log.cpp  idrivebmrserver/server_writer.cpp idrivebmrserver/server_running.cpp idrivebmrserver/server_cleanup.cpp idrivebmrserver/server_settings.cpp idrivebmrserver/server_update_stats.cpp idrivebmrserver/serverinterface/helper.cpp  idrivebmrserver/serverinterface/lastacts.cpp idrivebmrserver/serverinterface/login.cpp idrivebmrserver/serverinterface/progress.cpp idrivebmrserver/serverinterface/salt.cpp idrivebmrserver/serverinterface/users.cpp idrivebmrserver/serverinterface/piegraph.cpp idrivebmrserver/serverinterface/usage.cpp idrivebmrserver/serverinterface/usagegraph.cpp idrivebmrserver/serverinterface/status.cpp idrivebmrserver/serverinterface/settings.cpp idrivebmrserver/serverinterface/backups.cpp idrivebmrserver/serverinterface/logs.cpp idrivebmrserver/serverinterface/getimage.cpp idrivebmrserver/serverinterface/download_client.cpp idrivebmrserver/treediff/TreeDiff.cpp idrivebmrserver/treediff/TreeNode.cpp idrivebmrserver/treediff/TreeReader.cpp idrivebmrserver/ChunkPatcher.cpp idrivebmrserver/InternetServiceConnector.cpp idrivebmrserver/server_archive.cpp idrivebmrserver/filedownload.cpp idrivebmrserver/serverinterface/shutdown.cpp idrivebmrserver/snapshot_helper.cpp idrivebmrserver/verify_hashes.cpp idrivebmrserver/apps/cleanup_cmd.cpp idrivebmrserver/apps/repair_cmd.cpp idrivebmrserver/apps/md5sum_check.cpp idrivebmrserver/apps/patch.cpp idrivebmrserver/dao/ServerCleanupDao.cpp idrivebmrserver/lmdb/mdb.c idrivebmrserver/lmdb/midl.c idrivebmrserver/LMDBFileIndex.cpp idrivebmrserver/FileIndex.cpp idrivebmrserver/create_files_index.cpp idrivebmrserver/serverinterface/livelog.cpp idrivebmrserver/serverinterface/start_backup.cpp idrivebmrserver/serverinterface/create_zip.cpp idrivebmrserver/server_dir_links.cpp idrivebmrserver/dao/ServerBackupDao.cpp idrivebmrserver/apps/export_auth_log.cpp idrivebmrserver/apps/check_files_index.cpp idrivebmrserver/ServerDownloadThread.cpp idrivebmrserver/Backup.cpp idrivebmrserver/ImageBackup.cpp idrivebmrserver/FileBackup.cpp idrivebmrserver/IncrFileBackup.cpp idrivebmrserver/FullFileBackup.cpp idrivebmrserver/ContinuousBackup.cpp idrivebmrserver/ThrottleUpdater.cpp idrivebmrserver/FileMetadataDownloadThread.cpp idrivebmrserver/restore_client.cpp idrivebmrcommon/WalCheckpointThread.cpp idrivebmrserver/apps/skiphash_copy.cpp idrivebmrserver/cmdline_preprocessor.cpp idrivebmrserver/dao/ServerFilesDao.cpp idrivebmrserver/dao/ServerLinkDao.cpp idrivebmrserver/dao/ServerLinkJournalDao.cpp idrivebmrserver/serverinterface/add_client.cpp idrivebmrserver/serverinterface/restore_prepare_wait.cpp idrivebmrserver/copy_storage.cpp idrivebmrserver/ImageMount.cpp idrivebmrserver/DataplanDb.cpp idrivebmrserver/PhashLoad.cpp idrivebmrserver/serverinterface/scripts.cpp idrivebmrserver/Alerts.cpp idrivebmrserver/Mailer.cpp idrivebmrserver/LogReport.cpp

idrivebmrsrv_SOURCES += fileservplugin/dllmain.cpp fileservplugin/bufmgr.cpp fileservplugin/CClientThread.cpp fileservplugin/CriticalSection.cpp fileservplugin/CTCPFileServ.cpp fileservplugin/CUDPThread.cpp fileservplugin/FileServ.cpp fileservplugin/FileServFactory.cpp fileservplugin/log.cpp fileservplugin/main.cpp fileservplugin/map_buffer.cpp fileservplugin/pluginmgr.cpp fileservplugin/ChunkSendThread.cpp fileservplugin/PipeFile.cpp fileservplugin/PipeSessions.cpp fileservplugin/PipeFileUnix.cpp fileservplugin/PipeFileBase.cpp fileservplugin/FileMetadataPipe.cpp fileservplugin/PipeFileTar.cpp fileservplugin/PipeFileExt.cpp

if WITH_URLPLUGIN
idrivebmrsrv_SOURCES += urlplugin/dllmain.cpp urlplugin/pluginmgr.cpp urlplugin/UrlFactory.cpp
endif

if WITH_CRYPTOPLUGIN
idrivebmrsrv_SOURCES += cryptoplugin/dllmain.cpp cryptoplugin/AESDecryption.cpp cryptoplugin/CryptoFactory.cpp cryptoplugin/pluginmgr.cpp cryptoplugin/AESEncryption.cpp cryptoplugin/ZlibCompression.cpp cryptoplugin/ZlibDecompression.cpp cryptoplugin/AESGCMDecryption.cpp cryptoplugin/AESGCMEncryption.cpp cryptoplugin/ECDHKeyExchange.cpp

if EMBEDDED_CRYPTOPP
idrivebmrsrv_SOURCES += cryptoplugin/cryptlib.cpp cryptoplugin/algebra.cpp cryptoplugin/algparam.cpp cryptoplugin/asn.cpp cryptoplugin/basecode.cpp cryptoplugin/cbcmac.cpp cryptoplugin/channels.cpp cryptoplugin/dh.cpp cryptoplugin/dll.cpp cryptoplugin/dsa.cpp cryptoplugin/ec2n.cpp cryptoplugin/eccrypto.cpp cryptoplugin/ecp.cpp cryptoplugin/eprecomp.cpp cryptoplugin/files.cpp cryptoplugin/filters.cpp cryptoplugin/gf2n.cpp cryptoplugin/gfpcrypt.cpp cryptoplugin/hex.cpp cryptoplugin/hmac.cpp cryptoplugin/integer.cpp cryptoplugin/iterhash.cpp cryptoplugin/misc.cpp cryptoplugin/modes.cpp cryptoplugin/queue.cpp cryptoplugin/nbtheory.cpp cryptoplugin/oaep.cpp cryptoplugin/osrng.cpp cryptoplugin/pch.cpp cryptoplugin/pkcspad.cpp cryptoplugin/pubkey.cpp cryptoplugin/randpool.cpp cryptoplugin/rdtables.cpp cryptoplugin/rijndael.cpp cryptoplugin/rng.cpp cryptoplugin/rsa.cpp cryptoplugin/sha.cpp cryptoplugin/simple.cpp cryptoplugin/skipjack.cpp cryptoplugin/strciphr.cpp cryptoplugin/trdlocal.cpp cryptoplugin/cpu.cpp cryptoplugin/gzip.cpp cryptoplugin/gcm.cpp cryptoplugin/des.cpp cryptoplugin/authenc.cpp cryptoplugin/fips140.cpp cryptoplugin/zdeflate.cpp cryptoplugin/cmac.cpp cryptoplugin/eax.cpp cryptoplugin/adler32.cpp cryptoplugin/zinflate.cpp cryptoplugin/mqueue.cpp cryptoplugin/hrtimer.cpp cryptoplugin/pssr.cpp cryptoplugin/crc.cpp cryptoplugin/dessp.cpp cryptoplugin/zlib.cpp cryptoplugin/md5.cpp
endif

endif

idrivebmrsrv_SOURCES += luaplugin/dllmain.cpp luaplugin/LuaInterpreter.cpp luaplugin/src/all.c luaplugin/pluginmgr.cpp


if WITH_FUSEPLUGIN
idrivebmrsrv_SOURCES += fuseplugin/dllmain.cpp
endif

if WITH_FORTIFY
FORTIFY_FLAGS = -fstack-protector-strong --param=ssp-buffer-size=4 -Wformat -Werror=format-security -D_FORTIFY_SOURCE=2 -fPIE
FORTIFY_ldflags = -Wl,-z,relro
endif

idrivebmrsrv_LDADD = $(LIBCURL) $(PTHREAD_LIBS) $(DLOPEN_LIBS) $(FUSE_LIBS) -ljsoncpp
idrivebmrsrv_CXXFLAGS = $(PTHREAD_CFLAGS) -DLINUX -DSTATIC_PLUGIN $(WITH_FUSEPLUGIN_CXXFLAGS) -DVARDIR='"$(localstatedir)"' -DBINDIR='"$(bindir)"' -DDATADIR='"$(datadir)"'
idrivebmrsrv_CPPFLAGS = $(CRYPTOPP_CPPFLAGS) $(LIBCURL_CPPFLAGS) $(FUSE_CFLAGS) $(FORTIFY_FLAGS) -DSQLITE_PREPARE_RETRIES=5
if !WITH_ASSERTIONS
idrivebmrsrv_CPPFLAGS+=-DNDEBUG
endif
idrivebmrsrv_CFLAGS = -DSQLITE_ENABLE_UNLOCK_NOTIFY -DSQLITE_MAX_MMAP_SIZE=0x10000000000LL $(SUID_CFLAGS)
idrivebmrsrv_LDFLAGS = $(SUID_LDFLAGS) $(CRYPTOPP_LDFLAGS) $(FORTIFY_ldflags)

if !EMBEDDED_CRYPTOPP
idrivebmrsrv_LDADD += $(CRYPTOPP_LIBS)
else
idrivebmrsrv_CXXFLAGS += -DCRYPTOPP_INCLUDE_PREFIX=cryptoplugin -DCRYPTOPP_NO_UNALIGNED_DATA_ACCESS -DCRYPTOPP_NO_BACKWARDS_COMPATIBILITY_562
if TARGET_CPU_IS_X86
idrivebmrsrv_CXXFLAGS += -msse -msse2
endif
endif

if !WITH_CRYPTOPLUGIN
idrivebmrsrv_CPPFLAGS+=-DDO_NOT_USE_CRYPTOPP_SHA -DDO_NOT_USE_CRYPTOPP_MD5
endif



idrivebmr_snapshot_helper_SOURCES = snapshot_helper/main.cpp idrivebmrcommon/os_functions_lin_min.cpp stringtools.cpp
idrivebmr_snapshot_helper_LDADD = 
idrivebmr_snapshot_helper_CXXFLAGS=-DOS_FUNC_NO_NET -DOS_FUNC_NO_IFACE_SERVER -DLINUX
idrivebmr_snapshot_helper_CFLAGS=$(SUID_CFLAGS)
idrivebmr_snapshot_helper_CPPFLAGS=$(FORTIFY_FLAGS)
idrivebmr_snapshot_helper_LDFLAGS=$(SUID_LDFLAGS) $(FORTIFY_ldflags)

idrivebmr_mount_helper_SOURCES = mount_helper/main.cpp idrivebmrcommon/os_functions_lin_min.cpp stringtools.cpp 
idrivebmr_mount_helper_LDADD = 
idrivebmr_mount_helper_CXXFLAGS=-DOS_FUNC_NO_NET -DOS_FUNC_NO_IFACE_SERVER -DLINUX  $(WITH_FUSEPLUGIN_CXXFLAGS) -DBINDIR='"$(bindir)"'
idrivebmr_mount_helper_CFLAGS=$(SUID_CFLAGS)
idrivebmr_mount_helper_CPPFLAGS=$(FORTIFY_FLAGS)
idrivebmr_mount_helper_LDFLAGS=$(SUID_LDFLAGS) $(FORTIFY_ldflags)

SUBDIRS=. $(P_SUBDIRS)
install-exec-local: defaults_server init.d_server logrotate_idrivebmrsrv
if INSTALL_INITD
if PACKAGING
	$(INSTALL) -D "$(srcdir)/init.d_server" "$(DESTDIR)$(sysconfdir)/init.d/idrivebmrsrv"
	$(INSTALL) -D "$(srcdir)/defaults_server" "$(DESTDIR)$(sysconfdir)/default/idrivebmrsrv"
	$(INSTALL) -m 644 -D "$(srcdir)/logrotate_idrivebmrsrv" "$(DESTDIR)$(sysconfdir)/logrotate.d/idrivebmrsrv"
else
	$(INSTALL) -D "$(srcdir)/init.d_server" "$(sysconfdir)/init.d/idrivebmrsrv"
	$(INSTALL) -m 644 -D "$(srcdir)/logrotate_idrivebmrsrv" "$(sysconfdir)/logrotate.d/idrivebmrsrv"
	if ! test -e "$(sysconfdir)/default/idrivebmrsrv"; then $(INSTALL) -D "$(srcdir)/defaults_server" "$(sysconfdir)/default/idrivebmrsrv"; fi
endif
endif

	mkdir -p "$(DESTDIR)$(localstatedir)/idrivebmr"
	test "x$(DESTDIR)$(sbindir)" = "x$(DESTDIR)$(bindir)" || ! test -e $(DESTDIR)$(sbindir)/idrivebmrsrv || rm $(DESTDIR)$(sbindir)/idrivebmrsrv
if !PACKAGING
if FREEBSD
	pw groupadd idrivebmr -q || true
	pw useradd idrivebmr -q -g idrivebmr -d "$(DESTDIR)$(localstatedir)/idrivebmr" -m -s /usr/bin/false || true
else
	adduser idrivebmr --system --quiet --group --home "$(DESTDIR)$(localstatedir)/idrivebmr" || adduser idrivebmr --system --user-group --home "$(DESTDIR)$(localstatedir)/idrivebmr" || true
endif
	chown idrivebmr:idrivebmr "$(DESTDIR)$(localstatedir)/idrivebmr"
endif

install-data-local: idrivebmr/status.htm idrivebmrserver/www/js/*.js idrivebmrserver/www/js/vs/*.js idrivebmrserver/www/js/vs/base/worker/* idrivebmrserver/www/js/vs/basic-languages/src/* idrivebmrserver/www/js/vs/editor/* idrivebmrserver/www/*.htm idrivebmrserver/www/*.ico idrivebmrserver/www/css/*.css idrivebmrserver/www/images/*.png idrivebmrserver/www/images/*.gif idrivebmrserver/www/fonts/* idrivebmrserver/idrivebmr_ecdsa409k1.pub idrivebmrserver/www/swf/* server-license.txt idrivebmr/dataplan_db.txt
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(localstatedir)/idrivebmr"
	chmod +x "$(DESTDIR)$(localstatedir)/idrivebmr"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmr/dataplan_db.txt "$(DESTDIR)$(localstatedir)/idrivebmr/dataplan_db.txt"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr/www"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr/www/css"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr/www/js"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/base"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/base/worker"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/basic-languages"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/basic-languages/src"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/editor"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr/www/fonts"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr/www/images"
	$(INSTALL) $(INSTALL_OPTS) -m 744 -d "$(DESTDIR)$(datadir)/idrivebmr/www/swf"
if !PACKAGING
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr/www/css"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr/www/js"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/base"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/base/worker"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/basic-languages"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/basic-languages/src"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/editor"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr/www/fonts"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr/www/images"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr/www/swf"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr/www"
	chmod +x "$(DESTDIR)$(datadir)/idrivebmr"
endif
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/www/js/*.js "$(DESTDIR)$(datadir)/idrivebmr/www/js/"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/www/js/vs/*.js "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/www/js/vs/base/worker/* "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/base/worker/"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/www/js/vs/basic-languages/src/* "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/basic-languages/src/"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/www/js/vs/editor/* "$(DESTDIR)$(datadir)/idrivebmr/www/js/vs/editor/"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/www/*.htm "$(DESTDIR)$(datadir)/idrivebmr/www/"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/www/*.ico "$(DESTDIR)$(datadir)/idrivebmr/www/"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/www/css/*.css "$(DESTDIR)$(datadir)/idrivebmr/www/css/"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/www/images/*.png "$(DESTDIR)$(datadir)/idrivebmr/www/images/"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/www/images/*.gif "$(DESTDIR)$(datadir)/idrivebmr/www/images/"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/www/swf/* "$(DESTDIR)$(datadir)/idrivebmr/www/swf/"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/www/fonts/* "$(DESTDIR)$(datadir)/idrivebmr/www/fonts/"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/idrivebmrserver/idrivebmr_ecdsa409k1.pub "$(DESTDIR)$(datadir)/idrivebmr/idrivebmr_ecdsa409k1.pub"
	$(INSTALL_DATA) $(INSTALL_OPTS) $(srcdir)/server-license.txt "$(DESTDIR)$(datadir)/idrivebmr/server-license.txt"
if !PACKAGING
	chown idrivebmr:idrivebmr "$(DESTDIR)$(datadir)/idrivebmr"
	chown idrivebmr:idrivebmr "$(DESTDIR)$(datadir)/idrivebmr/idrivebmr_ecdsa409k1.pub"
	chown -R idrivebmr:idrivebmr "$(DESTDIR)$(datadir)/idrivebmr/www"
endif
	
install-exec-hook:
	chmod +s $(DESTDIR)$(bindir)/idrivebmr_snapshot_helper
	chmod +s $(DESTDIR)$(bindir)/idrivebmr_mount_helper
	
cryptoplugin_headers = cryptoplugin/AESEncryption.h cryptoplugin/AESDecryption.h cryptoplugin/IAESDecryption.h cryptoplugin/ICryptoFactory.h cryptoplugin/pluginmgr.h cryptoplugin/IAESEncryption.h cryptoplugin/CryptoFactory.h cryptoplugin/IZlibCompression.h cryptoplugin/IZlibDecompression.h cryptoplugin/ZlibCompression.h cryptoplugin/ZlibDecompression.h cryptoplugin/cryptopp_inc.h cryptoplugin/AESGCMDecryption.h cryptoplugin/AESGCMEncryption.h cryptoplugin/ECDHKeyExchange.h cryptoplugin/IAESGCMDecryption.h cryptoplugin/IAESGCMEncryption.h cryptoplugin/IECDHKeyExchange.h

fileservplugin_headers = fileservplugin/bufmgr.h fileservplugin/CUDPThread.h fileservplugin/FileServFactory.h fileservplugin/IFileServ.h fileservplugin/packet_ids.h fileservplugin/socket_header.h fileservplugin/CriticalSection.h fileservplugin/FileServ.h fileservplugin/log.h fileservplugin/pluginmgr.h   fileservplugin/CClientThread.h fileservplugin/CTCPFileServ.h fileservplugin/IFileServFactory.h fileservplugin/map_buffer.h fileservplugin/settings.h fileservplugin/types.h fileservplugin/chunk_settings.h fileservplugin/ChunkSendThread.h fileservplugin/PipeFile.h fileservplugin/PipeSessions.h  fileservplugin/PipeFileBase.h fileservplugin/IPermissionCallback.h fileservplugin/FileMetadataPipe.h fileservplugin/PipeFileTar.h fileservplugin/PipeFileExt.h

fsimageplugin_headers = fsimageplugin/filesystem.h fsimageplugin/FSImageFactory.h fsimageplugin/IFilesystem.h fsimageplugin/IFSImageFactory.h fsimageplugin/IVHDFile.h fsimageplugin/pluginmgr.h fsimageplugin/vhdfile.h fsimageplugin/fs/ntfs.h fsimageplugin/fs/unknown.h fsimageplugin/CompressedFile.h fsimageplugin/LRUMemCache.h common/miniz.h fsimageplugin/cowfile.h fsimageplugin/FileWrapper.h fsimageplugin/ClientBitmap.h 

tclap_headers = \
			 tclap/CmdLineInterface.h \
			 tclap/ArgException.h \
			 tclap/CmdLine.h \
			 tclap/XorHandler.h \
			 tclap/MultiArg.h \
			 tclap/UnlabeledMultiArg.h \
			 tclap/ValueArg.h \
			 tclap/UnlabeledValueArg.h \
			 tclap/Visitor.h tclap/Arg.h \
			 tclap/HelpVisitor.h \
			 tclap/SwitchArg.h \
			 tclap/MultiSwitchArg.h \
			 tclap/VersionVisitor.h \
			 tclap/IgnoreRestVisitor.h \
			 tclap/CmdLineOutput.h \
			 tclap/StdOutput.h \
			 tclap/DocBookOutput.h \
			 tclap/ZshCompletionOutput.h \
			 tclap/OptionalUnlabeledTracker.h \
			 tclap/Constraint.h \
			 tclap/ValuesConstraint.h \
			 tclap/ArgTraits.h \
			 tclap/StandardTraits.h

if EMBEDDED_CRYPTOPP
cryptopp_headers = cryptoplugin/*.h
else
cryptopp_headers =
endif

luaplugin_headers = luaplugin/ILuaInterpreter.h luaplugin/LuaInterpreter.h luaplugin/pluginmgr.h luaplugin/src/* luaplugin/lua/dkjson_lua.h
	
noinst_HEADERS=SessionMgr.h WorkerThread.h Helper_win32.h Database.h defaults.h ServiceAcceptor.h Query.h SettingsReader.h file.h file_memory.h MemorySettingsReader.h Condition_lin.h LookupService.h Template.h types.h DBSettingsReader.h stringtools.h ThreadPool.h libs.h vld_.h ServiceWorker.h StreamPipe.h LoadbalancerClient.h socket_header.h FileSettingsReader.h SelectThread.h md5.h vld.h Table.h Client.h MemoryPipe.h Mutex_lin.h AcceptThread.h OutputStream.h Server.h Interface/SessionMgr.h Interface/Service.h Interface/PluginMgr.h Interface/Database.h Interface/Pipe.h Interface/CustomClient.h Interface/User.h Interface/Query.h Interface/SettingsReader.h Interface/Types.h Interface/Template.h Interface/ThreadPool.h Interface/Mutex.h Interface/File.h Interface/Condition.h Interface/Table.h Interface/Plugin.h Interface/Thread.h Interface/Action.h Interface/Object.h Interface/OutputStream.h Interface/Server.h libfastcgi/fastcgi.hpp sqlite/sqlite3.h sqlite/sqlite3ext.h utf8/utf8.h utf8/utf8/checked.h utf8/utf8/core.h utf8/utf8/unchecked.h cryptoplugin/ICryptoFactory.h cryptoplugin/IAESEncryption.h cryptoplugin/IAESDecryption.h Interface/DatabaseFactory.h Interface/DatabaseInt.h SQLiteFactory.h sqlite/shell.h PipeThrottler.h Interface/PipeThrottler.h mt19937ar.h DatabaseCursor.h Interface/DatabaseCursor.h Interface/SharedMutex.h SharedMutex_lin.h httpserver/HTTPAction.h httpserver/HTTPClient.h httpserver/HTTPFile.h httpserver/HTTPProxy.h httpserver/HTTPService.h httpserver/IndexFiles.h httpserver/MIMEType.h idrivebmrserver/server_ping.h idrivebmrserver/server_cleanup.h idrivebmrcommon/os_functions.h idrivebmrcommon/json.h idrivebmrserver/serverinterface/helper.h idrivebmrserver/serverinterface/action_header.h idrivebmrserver/serverinterface/actions.h idrivebmrserver/server_writer.h idrivebmrcommon/settings.h idrivebmrserver/server_settings.h idrivebmrserver/zero_hash.h idrivebmrserver/server_update.h idrivebmrserver/server_log.h idrivebmrserver/server_hash.h idrivebmrserver/server_status.h idrivebmrcommon/bufmgr.h idrivebmrserver/server_update_stats.h idrivebmrcommon/sha2/sha2.h idrivebmrcommon/fileclient/FileClient.h common/data.h idrivebmrcommon/fileclient/socket_header.h idrivebmrcommon/fileclient/tcpstack.h idrivebmrcommon/fileclient/packet_ids.h idrivebmrserver/database.h idrivebmrserver/mbr_code.h idrivebmrserver/action_header.h idrivebmrcommon/escape.h idrivebmrserver/server.h idrivebmrserver/server_running.h idrivebmrserver/server_prepare_hash.h idrivebmrserver/actions.h idrivebmrserver/server_channel.h idrivebmrserver/ClientMain.h idrivebmrserver/treediff/TreeDiff.h idrivebmrserver/treediff/TreeNode.h idrivebmrserver/treediff/TreeReader.h fileservplugin/IFileServFactory.h fileservplugin/IFileServ.h urlplugin/IUrlFactory.h idrivebmrcommon/capa_bits.h cryptoplugin/ICryptoFactory.h idrivebmrcommon/fileclient/FileClientChunked.h idrivebmrserver/ChunkPatcher.h idrivebmrcommon/CompressedPipe.h idrivebmrcommon/InternetServicePipe.h idrivebmrcommon/InternetServicePipe2.h idrivebmrcommon/InternetServiceIDs.h idrivebmrserver/InternetServiceConnector.h md5.h idrivebmrcommon/settingslist.h idrivebmrserver/server_archive.h cryptoplugin/IZlibCompression.h cryptoplugin/IZlibDecompression.h cryptoplugin/ICryptoFactory.h cryptoplugin/IAESEncryption.h cryptoplugin/IAESDecryption.h fileservplugin/chunk_settings.h idrivebmrcommon/internet_pipe_capabilities.h idrivebmrcommon/mbrdata.h idrivebmrserver/filedownload.h idrivebmrserver/snapshot_helper.h idrivebmrserver/apps/cleanup_cmd.h idrivebmrserver/apps/repair_cmd.h idrivebmrserver/dao/ServerCleanupDao.h idrivebmrserver/lmdb/lmdb.h idrivebmrserver/lmdb/midl.h idrivebmrserver/LMDBFileIndex.h idrivebmrserver/create_files_index.h idrivebmrserver/FileIndex.h idrivebmrserver/serverinterface/rights.h idrivebmrserver/server_dir_links.h idrivebmrserver/dao/ServerBackupDao.h idrivebmrserver/apps/app.h idrivebmrserver/apps/export_auth_log.h idrivebmrserver/serverinterface/login.h idrivebmrserver/ServerDownloadThread.h common/adler32.h idrivebmrcommon/file_metadata.h idrivebmrcommon/filelist_utils.h idrivebmrserver/Backup.h idrivebmrserver/ImageBackup.h idrivebmrserver/FileBackup.h idrivebmrserver/IncrFileBackup.h idrivebmrserver/FullFileBackup.h idrivebmrserver/ContinuousBackup.h idrivebmrserver/ThrottleUpdater.h idrivebmrcommon/glob.h idrivebmrserver/FileMetadataDownloadThread.h idrivebmrserver/restore_client.h idrivebmrcommon/chunk_hasher.h idrivebmrcommon/WalCheckpointThread.h idrivebmrcommon/CompressedPipe2.h urlplugin/IUrlFactory.h urlplugin/pluginmgr.h urlplugin/UrlFactory.h StaticPluginRegistration.h $(cryptoplugin_headers) $(fileservplugin_headers) $(fsimageplugin_headers) $(tclap_headers) idrivebmrserver/backup_server_db.h idrivebmrcommon/SparseFile.h idrivebmrcommon/ExtentIterator.h idrivebmrserver/dao/ServerLinkDao.h idrivebmrserver/dao/ServerLinkJournalDao.h idrivebmrcommon/server_compat.h idrivebmrserver/dao/ServerFilesDao.h idrivebmrserver/apps/skiphash_copy.h idrivebmrserver/apps/check_files_index.h idrivebmrserver/apps/patch.h idrivebmrserver/serverinterface/backups.h idrivebmrserver/server_continuous.h idrivebmrcommon/change_ids.h  idrivebmrcommon/TreeHash.h idrivebmrserver/copy_storage.h idrivebmrserver/ImageMount.h common/bitmap.h $(cryptopp_headers) common/miniz.h idrivebmrserver/DataplanDb.h common/lrucache.h idrivebmrserver/PhashLoad.h fileservplugin/IPipeFileExt.h idrivebmrserver/Alerts.h idrivebmrserver/Mailer.h idrivebmrserver/alert_lua.h $(luaplugin_headers) idrivebmrserver/LogReport.h idrivebmrserver/report_lua.h

EXTRA_DIST=docs/idrivebmrsrv.1 init.d_server defaults_server logrotate_idrivebmrsrv idrivebmr-server.service idrivebmr-server-firewalld.xml idrivebmr/status.htm idrivebmrserver/www/js/*.js idrivebmrserver/www/js/vs/* idrivebmrserver/www/*.htm idrivebmrserver/www/*.ico idrivebmrserver/www/css/*.css idrivebmrserver/www/images/*.png idrivebmrserver/www/images/*.gif idrivebmrserver/www/*.ico idrivebmrserver/idrivebmr_ecdsa409k1.pub idrivebmrserver/www/swf/* idrivebmrserver/www/fonts/* tclap/COPYING tclap/AUTHORS server-license.txt idrivebmr/dataplan_db.txt
