////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
//  altar
//
//  author(s):
//  ENDESGA - https://twitter.com/ENDESGA | https://bsky.app/profile/endesga.bsky.social
//
//  https://github.com/H-language/altar
//  2026 - CC0 - FOSS forever
//

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region - DEPENDENCIES
//

#ifndef _GNU_SOURCE
	#define _GNU_SOURCE
#endif

#include <TUI.h>

#pragma endregion dependencies

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region - MACROS
//

#define system_tool_exists( NAME ) ( TUI_command( OS_PICK( "command -v " #NAME, "where " #NAME ) ) is 0 )

#pragma endregion macros

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region - CONSTANTS
//

#define ALTAR_NAME "altar"

////////////////////////////////////////////////////////////////
#pragma region - version

#define ALTAR_VERSION_MAJOR 0
#define ALTAR_VERSION_MINOR 1
#define ALTAR_VERSION_PATCH 0
#define ALTAR_VERSION_COMMIT 0
#define ALTAR_VERSION AS_BYTES( ALTAR_VERSION_MAJOR ) "." AS_BYTES( ALTAR_VERSION_MINOR ) "." AS_BYTES( ALTAR_VERSION_PATCH )

#pragma endregion

////////////////////////////////////////////////////////////////
#pragma region - limits

#define altar_max_projects 128
#define altar_max_libraries 64
#define altar_max_tools 32

#define altar_max_file_entries 8
#define altar_file_entry_max_elements 32
#define altar_file_element_max_size 64
#define altar_file_large_data_size ( altar_file_entry_max_elements * altar_file_element_max_size )

#pragma endregion limits

////////////////////////////////////////////////////////////////
#pragma region - names

#define libraries_bytes "libraries"
#define projects_bytes "projects"
#define tools_bytes "tools"

#define tool_tcc_bytes "tcc"
#define tool_gcc_bytes "gcc"
#define tool_upx_bytes "upx"
#define tool_vscode_bytes "vscode"
#define tool_formatter_bytes "formatter"

#pragma endregion names

#pragma endregion defaults

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region - DECLARATIONS
//

////////////////////////////////////////////////////////////////
#pragma region - file

fusion( altar_file_entry_data )
{
	byte elements[ altar_file_entry_max_elements ][ altar_file_element_max_size ];
	byte large_data[ altar_file_large_data_size ];
};

type( altar_file_entry )
{
	byte name[ 16 ];
	altar_file_entry_data data;

	n1 elements_count bits( altar_file_entry_max_elements );
	flag is_large bits_flag;
};

#pragma endregion file

////////////////////////////////////////////////////////////////
#pragma region - tools

group( altar_tool_type )
{
	altar_tool_tcc,
	#if OS_WINDOWS
		altar_tool_gcc,
	#endif
	altar_tool_upx,
	altar_tool_vscode,
	altar_tool_formatter,

	altar_tools_count
};

type( altar_tool )
{
	byte const ref name;

	byte version[ 16 ];
	byte repo[ 32 ];
	byte url_prefix[ 32 ];
	byte url_mid[ 24 ];
	byte url_suffix[ 24 ];

	n1 name_size bits( 16 );
	flag has_version bits_flag;
	flag needs_update bits_flag;
};

#pragma endregion tools

////////////////////////////////////////////////////////////////
#pragma region - ui

group( altar_state )
{
	altar_state_start,

	altar_state_libraries,
	altar_state_libraries_clone,
	altar_state_libraries_pull,

	altar_state_projects,
	altar_state_projects_new,
	altar_state_projects_setup,
	altar_state_projects_open,

	altar_state_tools,

	altar_states_count
};

type( altar_ui_item )
{
	byte const ref const name;
	fn_ref( anon, callback );
};

#pragma endregion ui

////////////////////////////////////////////////////////////////
#pragma region - altar

global
{
	byte curl_path[ path_max_size ];

	i1 selected_top;
	i1 selected_sub;

	byte projects[ altar_max_projects ][ path_max_size ];
	byte libraries[ altar_max_libraries ][ path_max_size ];
	byte libraries_valid_altar[ altar_max_libraries ];

	byte new_project_name[ path_max_size ];

	altar_file_entry file_entries[ altar_max_file_entries ];
	n1 file_entries_count bits( altar_max_file_entries );

	altar_state state bits( altar_states_count );
	n1 projects_count bits( altar_max_projects );
	n1 libraries_count bits( altar_max_libraries );

	flag get_tool_versions bits_flag;
}
altar;

#pragma endregion altar

#pragma endregion declarations

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region - DEFINITIONS
//

////////////////////////////////////////////////////////////////
#pragma region - file

fn altar_file_parse( byte const ref const file_bytes )
{
	altar.file_entries_count = 0;

	iter( entry, altar_max_file_entries )
	{
		altar.file_entries[ entry ].name[ 0 ] = eof_byte;
		altar.file_entries[ entry ].elements_count = 0;
		altar.file_entries[ entry ].is_large = no;
	}

	byte const ref file_bytes_ref = file_bytes;
	flag getting_elements = no;

	n2 current_entry = 0;
	n2 entry_name_size = 0;
	n2 entry_element_size = 0;
	n2 entry_large_data_size = 0;

	#define altar_file_current_entry altar.file_entries[ current_entry ]
	#define entry_name_add() altar_file_current_entry.name[ entry_name_size++ ] = val_of( file_bytes_ref++ )
	#define entry_element_add() altar_file_current_entry.data.elements[ altar_file_current_entry.elements_count ][ entry_element_size++ ] = val_of( file_bytes_ref++ )
	#define entry_large_data_add() altar_file_current_entry.data.large_data[ entry_large_data_size++ ] = val_of( file_bytes_ref++ )

	process_entry_start:
	{
		with( val_of( file_bytes_ref ) )
		{
			when( eof_byte )
			{
				out;
			}

			when( ' ', tab_byte, '\r', newline_byte )
			{
				++file_bytes_ref;
				jump process_entry_start;
			}

			other skip;
		}
	}

	process_entry:
	{
		with( val_of( file_bytes_ref ) )
		{
			when( eof_byte )
			{
				out;
			}

			other
			{
				if( getting_elements is no )
				{
					current_entry = altar.file_entries_count;
					altar.file_entries_count += 1;
					entry_name_add();

					process_entry_name:
					{
						with( val_of( file_bytes_ref ) )
						{
							when( eof_byte )
							{
								out;
							}

							when( ' ', tab_byte )
							{
								++file_bytes_ref;
								jump process_entry;
							}

							when( '\r' )
							{
								if( val_of( file_bytes_ref + 1 ) is newline_byte )
								{
									++file_bytes_ref;
								}
							} // fall through
							when( newline_byte )
							{
								altar_file_current_entry.name[ entry_name_size ] = eof_byte;
								entry_name_size = 0;
								getting_elements = yes;
								++file_bytes_ref;
								jump process_entry;
							}

							other
							{
								entry_name_add();
								jump process_entry_name;
							}
						}
					}
				}
				else
				{
					with( val_of( file_bytes_ref ) )
					{
						when( eof_byte )
						{
							out;
						}

						when( tab_byte )
						{
							++file_bytes_ref;
							skip;
						}

						when( '`' )
						{
							++file_bytes_ref;

							if( val_of( file_bytes_ref ) is '\r' ) ++file_bytes_ref;
							if( val_of( file_bytes_ref ) is newline_byte ) ++file_bytes_ref;

							process_large_entry:
							{
								with( val_of( file_bytes_ref ) )
								{
									when( eof_byte )
									{
										out;
									}

									when( '`' )
									{
										altar_file_current_entry.is_large = yes;
										altar_file_current_entry.elements_count += 1;
										altar_file_current_entry.data.large_data[ entry_large_data_size ] = eof_byte;
										++file_bytes_ref;
										jump next_entry;
									}

									other
									{
										entry_large_data_add();
										jump process_large_entry;
									}
								}
							}
						}

						other
						{
							next_entry:
							getting_elements = no;
							entry_element_size = 0;
							jump process_entry_start;
						}
					}

					process_element:
					{
						with( val_of( file_bytes_ref ) )
						{
							when( '\r' )
							{
								if( val_of( file_bytes_ref + 1 ) is newline_byte )
								{
									++file_bytes_ref;
								}
							} // fall through

							when( eof_byte, newline_byte )
							{
								jump_if( entry_element_size is 0 ) process_entry;
								//
								altar_file_current_entry.data.elements[ altar_file_current_entry.elements_count ][ entry_element_size ] = eof_byte;
								altar_file_current_entry.elements_count += 1;
								entry_element_size = 0;
								++file_bytes_ref;
								jump process_entry;
							}

							other
							{
								entry_element_add();
								jump process_element;
							}
						}
					}
				}
			}
		}
	}
}

embed n2 const altar_file_find( byte const ref const entry_name )
{
	n2 const entry_name_size = bytes_measure( entry_name );
	iter( entry, altar.file_entries_count )
	{
		if( bytes_match( entry_name, altar.file_entries[ entry ].name, entry_name_size ) )
		{
			out entry;
		}
	}
	out n2_max_val;
}

#pragma endregion file

////////////////////////////////////////////////////////////////
#pragma region - tools

////////////////////////////////
#pragma region | tools / hidden

#define _altar_tool_set( NAME, HAS_VERSION, REPO, PREFIX, MID, SUFFIX )[ altar_tool_##NAME ] = { .name = tool_##NAME##_bytes , .name_size = size_of_bytes( tool_##NAME##_bytes ) , .has_version = HAS_VERSION , .repo = REPO , .url_prefix = PREFIX , .url_mid = MID , .url_suffix = SUFFIX }

#pragma endregion hidden

////////////////////////////////
#pragma region | tools / visible

perm altar_tool altar_tools[ altar_tools_count ] =
	{
		_altar_tool_set( tcc, no, "TinyCC/tinycc", "", "", ".git" ),
		#if OS_WINDOWS
			_altar_tool_set( gcc, yes, "ENDESGA/TinyGW", "/releases/download/", "/TinyGW-", "-x86_64.7z" ),
		#endif
		_altar_tool_set( upx, yes, "upx/upx", "/releases/download/v", "/upx-", OS_PICK( "-amd64_linux.tar.xz", "-win64.zip" ) ),
		_altar_tool_set( vscode, yes, "VSCodium/vscodium", "/releases/download/", OS_PICK( "/VSCodium-linux-x64-", "/VSCodium-win32-x64-" ), OS_PICK( ".tar.gz", ".zip" ) ),
		_altar_tool_set( formatter, yes, "H-language/formatter", "/releases/download/", "/" tool_formatter_bytes, OS_PICK( "", ".exe" ) ),
	};

#pragma endregion visible

#pragma endregion tools

////////////////////////////////////////////////////////////////
#pragma region - altar

////////////////////////////////
#pragma region | altar / hidden

////////////////
#pragma region | - helpers

fn _altar_print_under( n1 const button_index )
{
	TUI_button const ref const b = ref_of( TUI.buttons[ button_index ] );
	n1 const target = ( b->x + b->size / 2 ) - 1;
	repeat( target )
	{
		TUI_print( "_" );
	}
	TUI_print( "/" );
}

fn _altar_print_bar( altar_ui_item const ref const items, i1 const items_count, i1 const item_active, byte const ref const primary_bytes, byte const ref const secondary_bytes )
{
	byte const ref const inactive_bytes = pick( item_active >= 0, secondary_bytes, "<w>" );

	iter( item_id, items_count )
	{
		TUI_print( secondary_bytes );

		if( item_id > 0 )
		{
			TUI_print( " " );
		}

		if( item_id is item_active )
		{
			TUI_print( "[ " );
			TUI_print( primary_bytes );
		}
		else if( item_active >= 0 and item_id is ( item_active + 1 ) )
		{
			TUI_print( "] " );
			TUI_print( inactive_bytes );
		}
		else
		{
			TUI_print( "| " );
			TUI_print( inactive_bytes );
		}

		TUI_print_button( items[ item_id ].name, items[ item_id ].callback );
	}
	TUI_print( secondary_bytes );
	TUI_print( pick( item_active is ( items_count - 1 ), " ]", " |" ) );
}

fn _altar_set_state( altar_state const state, i1 const top_id, i1 const sub_id )
{
	altar.state = state;
	altar.selected_top = top_id;
	altar.selected_sub = sub_id;
	TUI_reprint();
}

#pragma endregion helpers

////////////////
#pragma region | - macros

#define altar_set_state( STATE, TOP_ID, SUB_ID... ) out_if( altar.state is altar_state_##STATE ); _altar_set_state( altar_state_##STATE, TOP_ID, DEFAULT( -1, SUB_ID ) )

#define altar_sleep_reset() sleep( 1000 ); altar_set_state( start, -1 )
#define altar_sleep_back( STATE, TOP_ID ) altar_sleep_reset(); altar_set_state( STATE, TOP_ID )

#define altar_update_list( NAME ) altar.NAME##_count = os_get_folders( AS_BYTES( NAME ), altar.NAME, altar_max_##NAME, no );

#define altar_update_projects() if( altar.state isnt altar_state_##STATE )

#pragma endregion macros

#pragma endregion hidden

////////////////////////////////
#pragma region | altar / visible

////////////////
#pragma region | - exit

fn altar_exit()
{
	exit( success );
}

#pragma endregion exit

////////////////
#pragma region | - libraries

fn altar_set_state_libraries()
{
	altar_set_state( libraries, 0 );
	os_create_folder( libraries_bytes );
}

fn altar_set_state_libraries_clone()
{
	altar_set_state( libraries_clone, 0, 0 );
}

fn altar_set_state_libraries_pull()
{
	altar_set_state( libraries_pull, 0, 1 );
	altar_update_list( libraries );
}

#pragma endregion libraries

////////////////
#pragma region | - projects

fn altar_set_state_projects()
{
	altar_set_state( projects, 1 );
	os_create_folder( projects_bytes );
}

fn altar_set_state_projects_new()
{
	altar_set_state( projects_new, 1, 0 );
}

fn altar_set_state_projects_setup()
{
	altar_set_state( projects_setup, 1, 1 );
	altar_update_list( projects );
}

fn altar_set_state_projects_open()
{
	altar_set_state( projects_open, 1, 2 );
	altar_update_list( projects );
}

#pragma endregion projects

////////////////
#pragma region | - tools

fn altar_set_state_tools()
{
	os_create_folder( tools_bytes );

	if( altar.get_tool_versions is no )
	{
		TUI_print( newline "<y>getting versions for all tools..." );
		TUI_update_now();
		//
		iter( tool, altar_tools_count )
		{
			if( altar_tools[ tool ].has_version is no )
			{
				byte command[ KB( 1 ) ];
				byte ref command_ref = command;
				bytes_paste_move( command_ref, "git ls-remote https://github.com/" );
				bytes_paste_move( command_ref, altar_tools[ tool ].repo );
				bytes_paste_move( command_ref, " HEAD && git -C " tools_bytes separator );
				n1 skip_user = 0;
				while( val_of( altar_tools[ tool ].repo + skip_user++ ) isnt '/' );
				bytes_paste_move( command_ref, altar_tools[ tool ].repo + skip_user );
				bytes_paste_move( command_ref, " rev-parse HEAD" );
				bytes_end( command_ref );

				byte shas[ 2 ][ 128 ] = { 0 };
				n1 line_index = 0;
				os_handle command_handle = command_read_open_silent( command );
				byte line[ 128 ];
				while( os_handle_get_line( line, size_of_bytes( line ), command_handle ) isnt nothing )
				{
					bytes_paste( shas[ line_index ], line );
					line_index += 1;
					skip_if( line_index >= 2 );
				}
				command_read_close_silent( command_handle );

				altar_tools[ tool ].needs_update = os_folder_exists( tools_bytes separator "tinycc" ) and( bytes_compare( shas[ 0 ], shas[ 1 ], 12 ) isnt 0 );
				next;
			}
			//
			byte command[ KB( 1 ) ];
			byte ref command_ref = command;
			bytes_paste_move( command_ref, "curl -L -s https://api.github.com/repos/" );
			bytes_paste_move( command_ref, altar_tools[ tool ].repo );
			bytes_paste_move( command_ref, "/releases/latest" );
			bytes_end( command_ref );
			os_handle command_output_handle = command_read_open_silent( command );
			byte line_output[ KB( 1 ) ];
			byte ref version_ref = altar_tools[ tool ].version;
			while( os_handle_get_line( line_output, size_of_bytes( line_output ), command_output_handle ) isnt nothing )
			{
				byte const ref line_output_ref = line_output;
				process_output_byte:
				{
					with( val_of( line_output_ref ) )
					{
						when( ' ', tab_byte, '"' )
						{
							++line_output_ref;
							jump process_output_byte;
						}
						//
						when( '\r' )
						{
							if( val_of( line_output_ref + 1 ) is newline_byte )
							{
								++line_output_ref;
							}
						} // fall through
						when( newline_byte, eof_byte )
						{
							next;
						}
						//
						when( 't' )
						{
							if( bytes_match( line_output_ref, "tag_name", 8 ) )
							{
								line_output_ref += 9;
								process_tag_name:
								{
									with( val_of( line_output_ref ) )
									{
										when( ' ', tab_byte, '"', ':', 'v' )
										{
											++line_output_ref;
											jump process_tag_name;
										}
										//
										other
										{
											process_version:
											{
												with( val_of( line_output_ref ) )
												{
													when( '"', '\r', newline_byte )
													{
														val_of( version_ref ) = eof_byte;
														jump found_version;
													}
													//
													other
													{
														val_of( version_ref++ ) = val_of( line_output_ref++ );
														jump process_version;
													}
												}
											}
										}
									}
								}
							}
							else
							{
								next;
							}
						}
						//
						other next;
					}
				}
			}
			found_version:
			command_read_close_silent( command_output_handle );

			flag is_installed = no;
			byte version_command[ KB( 1 ) ];
			byte ref version_command_ref = version_command;
			with( altar_tools[ tool ].name[ 0 ] )
			{
				#if OS_WINDOWS
					when( 'g' )
					{
						is_installed = os_file_exists( path( tools_bytes, "tinygw", "bin", "gcc.exe" ) );
						bytes_paste_move( version_command_ref, path( tools_bytes, "tinygw", "bin", tool_gcc_bytes ) " --version" );
						skip;
					}
				#endif
				when( 'u' )
				{
					is_installed = os_file_exists( path( tools_bytes, tool_upx_bytes, PICK( OS_WINDOWS, "upx.exe", tool_upx_bytes ) ) );
					bytes_paste_move( version_command_ref, path( tools_bytes, tool_upx_bytes, tool_upx_bytes ) " --version" );
					skip;
				}
				when( 'v' )
				{
					is_installed = os_file_exists( path( tools_bytes, tool_vscode_bytes, "bin", OS_PICK( "codium", "codium.cmd" ) ) );
					bytes_paste_move( version_command_ref, path( tools_bytes, tool_vscode_bytes, "bin", OS_PICK( "codium", "codium.cmd" ) ) " --version" );
					skip;
				}
				when( 'f' )
				{
					is_installed = os_file_exists( path( tools_bytes, tool_formatter_bytes, tool_formatter_bytes ) OS_PICK( "", ".exe" ) );
					bytes_paste_move( version_command_ref, path( tools_bytes, tool_formatter_bytes, tool_formatter_bytes ) OS_PICK( "", ".exe" ) " --version" );
					skip;
				}
				other skip;
			}

			if( not is_installed )
			{
				altar_tools[ tool ].needs_update = no;
				next;
			}

			bytes_end( version_command_ref );

			os_handle version_handle = command_read_open_silent( version_command );
			byte version_line[ KB( 1 ) ];
			flag found_installed = no;
			while( os_handle_get_line( version_line, size_of_bytes( version_line ), version_handle ) isnt nothing )
			{
				next_if( found_installed );

				n2 const ls = bytes_measure( version_line );
				n2 const vs = bytes_measure( altar_tools[ tool ].version );
				if( vs > 0 and ls >= vs )
				{
					iter( index, ( ls - vs ) + 1 )
					{
						if( bytes_match( version_line + index, altar_tools[ tool ].version, vs ) )
						{
							found_installed = yes;
							skip;
						}
					}
				}
			}
			command_read_close_silent( version_handle );
			altar_tools[ tool ].needs_update = not found_installed;

			next;
		}

		TUI_print( " <w>done!" );
		TUI_update_now();
		sleep( 250 );

		altar.get_tool_versions = yes;
	}

	altar_set_state( tools, 2 );
}

#pragma endregion tools

////////////////
#pragma region | - data

perm altar_ui_item altar_top_data[] =
	{
		{
			libraries_bytes,
			altar_set_state_libraries
		},
		{
			projects_bytes,
			altar_set_state_projects
		},
		{
			tools_bytes,
			altar_set_state_tools
		},
	};

perm altar_ui_item const altar_libraries_data[] =
	{
		{
			"clone",
			altar_set_state_libraries_clone
		},
		{
			"pull",
			altar_set_state_libraries_pull
		},
	};

perm altar_ui_item const altar_projects_data[] =
	{
		{
			"new",
			altar_set_state_projects_new
		},
		{
			"setup",
			altar_set_state_projects_setup
		},
		{
			"open",
			altar_set_state_projects_open
		},
	};

#pragma endregion data

////////////////
#pragma region | - buttons

fn altar_library_clone()
{
	byte ref const library_url = TUI.buttons[ TUI.hover_target ].data;

	byte const ref const path_name = path_get_name( library_url );
	byte library_path[ path_max_size ];
	byte ref library_path_ref = library_path;
	bytes_paste_move( library_path_ref, libraries_bytes );
	bytes_separator_move( library_path_ref );
	bytes_paste_move( library_path_ref, path_name );
	bytes_end( library_path_ref );
	if( os_folder_exists( library_path ) )
	{
		TUI_newline();
		TUI_print( "<c>clone <m>failure: <y>" );
		TUI_print( library_url );
		TUI_print( " <c>already exists!" );
		TUI_update_now();

		jump sleep_exit;
	}

	byte command[ path_max_size ];
	byte ref command_ref = command;
	bytes_paste_move( command_ref, "git clone https://github.com/" );
	bytes_paste_move( command_ref, library_url );
	bytes_paste_move( command_ref, ".git " );
	bytes_paste_move( command_ref, library_path );
	bytes_end( command_ref );
	TUI_newline();
	TUI_print( "<c>cloning <y>" );
	TUI_print( library_url );
	TUI_print( "<m>... " );
	TUI_update_now();
	TUI_command( command );

	if( os_folder_exists( library_path ) )
	{
		TUI_print( "<w>done!" );
		TUI_update_now();
	}
	else
	{
		TUI_print( "<m>failure" );
		TUI_newline();
		TUI_print( "<m>invalid git repo!" );
		TUI_update_now();
	}

	sleep_exit:
	altar_sleep_reset();
}

fn altar_library_pull()
{
	byte ref const library_url = TUI.buttons[ TUI.hover_target ].data;
	n2 const library_url_size = bytes_measure( library_url );

	flag all = bytes_match( library_url, "all", 4 );
	n2 pull_start = 0;
	n2 pull_end = altar.libraries_count - 1;

	if( all is no )
	{
		iter( library, altar.libraries_count )
		{
			if( bytes_match( library_url, altar.libraries[ library ], library_url_size ) )
			{
				pull_start = library;
				pull_end = library;
				skip;
			}
		}
	}

	print_newline();

	byte command[ path_max_size ];
	range( library, pull_start, pull_end )
	{
		byte ref command_ref = command;
		bytes_paste_move( command_ref, "cd " libraries_bytes separator );
		bytes_paste_move( command_ref, altar.libraries[ library ] );
		bytes_paste_move( command_ref, "&& git pull && cd .." );
		bytes_end( command_ref );
		TUI_newline();
		TUI_print( "<c>pulling <y>" );
		TUI_print( altar.libraries[ library ] );
		TUI_print( "<m>... " );
		TUI_update_now();
		TUI_command( command );
		TUI_print( "<w>done!" );
		TUI_update_now();
	}

	TUI_print( "<c>" );

	altar_sleep_back( libraries, 0 );
}

fn altar_project_new_framework()
{
	byte const ref const framework_name = TUI.buttons[ TUI.hover_target ].data;
	n2 const framework_name_size = bytes_measure( framework_name );

	flag found = no;
	n2 found_index = 0;
	iter( library, altar.libraries_count )
	{
		if( bytes_match( framework_name, altar.libraries[ library ], framework_name_size ) )
		{
			found = yes;
			found_index = library;
			skip;
		}
	}
	out_if( found is no );
	out_if( altar.libraries_valid_altar[ found_index ] is no );

	byte new_path[ path_max_size ];
	byte ref new_path_ref = new_path;
	bytes_paste_move( new_path_ref, projects_bytes );
	bytes_separator_move( new_path_ref );
	bytes_paste_move( new_path_ref, altar.new_project_name );
	bytes_end( new_path_ref );

	os_create_folder( new_path );

	bytes_separator_move( new_path_ref );
	byte ref const base_path_ref = new_path_ref;

	//

	byte framework_path[ path_max_size ];
	byte ref framework_path_ref = framework_path;
	bytes_paste_move( framework_path_ref, libraries_bytes );
	bytes_separator_move( framework_path_ref );
	bytes_paste_move( framework_path_ref, framework_name );
	bytes_separator_move( framework_path_ref );
	bytes_paste_move( framework_path_ref, "framework.altar" );
	bytes_end( framework_path_ref );
	os_file framework_altar = os_map_file( framework_path );
	altar_file_parse( framework_altar.mapped_bytes );
	os_file_ref_unmap( ref_of( framework_altar ) );

	//

	// PROJECT.h
	{
		n2 const example_index = altar_file_find( "example" );
		//
		bytes_paste_move( new_path_ref, altar.new_project_name );
		bytes_paste_move( new_path_ref, ".h" );
		bytes_end( new_path_ref );
		os_file project_h = os_create_file( new_path, new_path_ref - new_path );
		os_file_ref_save( ref_of( project_h ), altar.file_entries[ example_index ].data.large_data, bytes_measure( altar.file_entries[ example_index ].data.large_data ) );
		os_file_ref_close( ref_of( project_h ) );
		new_path_ref = base_path_ref;
	}
	//

	// setup.altar
	{
		n2 const properties_index = altar_file_find( "properties" );
		//
		byte settings_bytes[ KB( 1 ) ];
		byte ref settings_bytes_ref = settings_bytes;
		bytes_paste_move( settings_bytes_ref, "framework" newline tab );
		bytes_paste_move( settings_bytes_ref, altar.file_entries[ properties_index ].data.elements[ 0 ] );
		bytes_paste_move( settings_bytes_ref, newline newline libraries_bytes newline tab newline newline "properties" newline tab "DEVELOPER_NAME" newline tab "0.1.0" newline );
		bytes_end( settings_bytes_ref );
		//
		bytes_paste_move( new_path_ref, "setup.altar" );
		bytes_end( new_path_ref );
		os_file settings_altar = os_create_file( new_path, new_path_ref - new_path );
		os_file_ref_save( ref_of( settings_altar ), settings_bytes, settings_bytes_ref - settings_bytes );
		os_file_ref_close( ref_of( settings_altar ) );
		new_path_ref = base_path_ref;
	}

	TUI_newline();
	TUI_print( "<c>new: <y>" );
	TUI_print( altar.new_project_name );
	TUI_print( " <m>project made!" );
	TUI_update_now();
	altar_sleep_reset();
}

fn altar_project_new_yes()
{
	TUI.input_bytes[ 0 ] = eof_byte;
	TUI.input_bytes_count = 0;

	altar_update_list( libraries );

	TUI_newline();
	TUI_print( "<c>| framework:" newline );
	TUI_print( "<c>| <w>" );

	byte framework_path[ path_max_size ];
	bytes_paste( framework_path, libraries_bytes );
	byte ref framework_path_ref = framework_path + bytes_measure( libraries_bytes );
	bytes_separator_move( framework_path_ref );
	byte ref framework_path_base_ref = framework_path_ref;

	iter( library, altar.libraries_count )
	{
		bytes_paste_move( framework_path_ref, altar.libraries[ library ] );
		bytes_separator_move( framework_path_ref );
		bytes_paste_move( framework_path_ref, "framework.altar" );
		bytes_end( framework_path_ref );

		if( os_file_exists( framework_path ) )
		{
			altar.libraries_valid_altar[ library ] = yes;
			if( library isnt 0 )
			{
				TUI_print( "<c>, <w>" );
			}
			TUI_print_button( altar.libraries[ library ], altar_project_new_framework, altar.libraries[ library ] );
		}
		else
		{
			altar.libraries_valid_altar[ library ] = no;
		}

		framework_path_ref = framework_path_base_ref;
	}
	TUI_newline();
	TUI_print( "<c>> <w>" );
}

fn altar_project_new()
{
	byte const ref const project_name = TUI.buttons[ TUI.hover_target ].data;
	bytes_paste( altar.new_project_name, project_name );

	byte new_path[ path_max_size ];
	byte ref new_path_ref = new_path;
	bytes_paste_move( new_path_ref, projects_bytes );
	bytes_separator_move( new_path_ref );
	bytes_paste_move( new_path_ref, altar.new_project_name );
	bytes_end( new_path_ref );

	TUI.input_bytes[ 0 ] = eof_byte;
	TUI.input_bytes_count = 0;

	if( os_folder_exists( new_path ) )
	{
		TUI_print( " <m>already exists!\n<y>overwrite? <w>" );
		TUI_print_button( "yes", altar_project_new_yes, project_name );
		TUI_print( " <m>/ <w>" );
		TUI_print_button( "no", altar_set_state_projects_new );
		TUI_newline();
		TUI_print( "<c>> <w>" );
	}
	else
	{
		altar_project_new_yes();
	}
}

fn altar_project_setup()
{
	byte const ref const name = TUI.buttons[ TUI.hover_target ].data;
	n1 const name_size = TUI.buttons[ TUI.hover_target ].size;

	#define TCC_PATH path( "..", "..", tools_bytes, "tinycc" )
	#define TCC_EXE path( TCC_PATH, tool_tcc_bytes )

	#define GCC_PATH OS_PICK( "", path( "..", "..", tools_bytes, "tinygw" ) )
	#define GCC_EXE OS_PICK( tool_gcc_bytes, path( GCC_PATH, "bin", tool_gcc_bytes ) )

	// project base path
	byte new_path[ path_max_size ];
	byte ref new_path_ref = new_path;
	bytes_paste_move( new_path_ref, projects_bytes );
	bytes_separator_move( new_path_ref );
	bytes_paste_move( new_path_ref, name );
	bytes_separator_move( new_path_ref );
	bytes_end( new_path_ref );
	byte ref const base_path_ref = new_path_ref;

	// find "framework" entry in setup file
	bytes_paste_move( new_path_ref, "setup.altar" );
	bytes_end( new_path_ref );
	if( not os_file_exists( new_path ) )
	{
		new_path_ref = base_path_ref;
		bytes_paste_move( new_path_ref, "framework.altar" );
		bytes_end( new_path_ref );
		if( not os_file_exists( new_path ) )
		{
			TUI_newline();
			TUI_print( "<c>setup: <y>setup.altar <m>not found!" );
			TUI_update_now();
			altar_sleep_back( projects, 1 );
			out;
		}
	}

	os_file setup_altar = os_map_file( new_path );
	altar_file_parse( setup_altar.mapped_bytes );
	os_file_ref_unmap( ref_of( setup_altar ) );
	new_path_ref = base_path_ref;
	n2 const setup_framework_index = altar_file_find( "framework" );
	n2 const setup_libraries_index = altar_file_find( libraries_bytes );
	n2 const setup_properties_index = altar_file_find( "properties" );

	flag const has_framework = setup_framework_index isnt n2_max_val;
	flag const has_properties = setup_properties_index isnt n2_max_val;

	if( not has_framework and not has_properties )
	{
		TUI_newline();
		TUI_print( "<c>setup: <y>setup.altar <m>has no framework or properties!" );
		TUI_update_now();
		altar_sleep_back( projects, 1 );
		out;
	}

	byte setup_version[ 16 ] = "0.1.0";
	byte ref setup_version_ref = setup_version;
	n1 setup_version_size = bytes_measure( setup_version );
	if( setup_properties_index isnt n2_max_val )
	{
		setup_version_size = bytes_measure( altar.file_entries[ setup_properties_index ].data.elements[ 1 ] );
		bytes_copy_move( setup_version_ref, altar.file_entries[ setup_properties_index ].data.elements[ 1 ], setup_version_size );
		bytes_end( setup_version_ref );
	}

	// make .gitattributes
	{
		bytes_paste_move( new_path_ref, ".gitattributes" );
		bytes_end( new_path_ref );
		if( not os_file_exists( new_path ) )
		{
			byte gitattributes_bytes[] = "* text=auto eol=lf" newline;
			//
			os_file gitattributes = os_create_file( new_path, new_path_ref - new_path );
			os_file_ref_save( ref_of( gitattributes ), gitattributes_bytes, size_of_bytes( gitattributes_bytes ) );
			os_file_ref_close( ref_of( gitattributes ) );
		}
		new_path_ref = base_path_ref;
	}

	if( has_framework is yes )
	{
		// make .gitignore
		{
			bytes_paste_move( new_path_ref, ".gitignore" );
			bytes_end( new_path_ref );
			if( not os_file_exists( new_path ) )
			{
				byte gitignore_bytes[ KB( 1 ) ];
				byte ref gitignore_bytes_ref = gitignore_bytes;
				bytes_paste_move( gitignore_bytes_ref, "compile_*" newline ".vscode" newline "*.def" newline "*.exe" newline "*.rc" newline "*.ico" newline );
				bytes_paste_move( gitignore_bytes_ref, name );
				bytes_newline_move( gitignore_bytes_ref );
				bytes_paste_move( gitignore_bytes_ref, name );
				bytes_paste_move( gitignore_bytes_ref, "_debug" );
				bytes_newline_move( gitignore_bytes_ref );
				bytes_paste_move( gitignore_bytes_ref, name );
				bytes_paste_move( gitignore_bytes_ref, "_uncompressed" );
				bytes_newline_move( gitignore_bytes_ref );
				bytes_end( gitignore_bytes_ref );
				//
				os_file gitignore = os_create_file( new_path, new_path_ref - new_path );
				os_file_ref_save( ref_of( gitignore ), gitignore_bytes, gitignore_bytes_ref - gitignore_bytes );
				os_file_ref_close( ref_of( gitignore ) );
			}
			new_path_ref = base_path_ref;
		}

		byte ref const framework_name = path_get_name( altar.file_entries[ setup_framework_index ].data.elements[ 0 ] );

		// find framework
		byte framework_path[ path_max_size ];
		byte ref framework_path_ref = framework_path;
		bytes_paste_move( framework_path_ref, libraries_bytes separator );
		bytes_paste_move( framework_path_ref, framework_name );
		bytes_end( framework_path_ref );
		if( not os_folder_exists( framework_path ) )
		{
			byte const ref default_inputs[] =
				{
					libraries_bytes,
					altar_libraries_data[ 0 ].name,
					altar.file_entries[ setup_framework_index ].data.elements[ 0 ]
				};

			TUI_click_buttons( default_inputs, size_of_array( default_inputs ) );
		}

		bytes_separator_move( framework_path_ref );
		bytes_paste_move( framework_path_ref, "framework.altar" );
		bytes_end( framework_path_ref );
		if( not os_file_exists( framework_path ) )
		{
			TUI_newline();
			TUI_print( "<c>setup: <y>framework.altar <m>not found!" );
			TUI_update_now();
			altar_sleep_back( projects, 1 );
			out;
		}

		// find setup libraries
		byte setup_libraries[ altar_file_entry_max_elements ][ altar_file_element_max_size ];
		n2 setup_libraries_count = 0;
		if( setup_libraries_index isnt n2_max_val )
		{
			setup_libraries_count = altar.file_entries[ setup_libraries_index ].elements_count;
			iter( element, setup_libraries_count )
			{
				byte ref libraries_element_ref = altar.file_entries[ setup_libraries_index ].data.elements[ element ];
				byte const ref const library_name = path_get_name( libraries_element_ref );

				byte library_path[ path_max_size ];
				byte ref library_path_ref = library_path;
				bytes_paste_move( library_path_ref, libraries_bytes separator );
				bytes_paste_move( library_path_ref, library_name );
				bytes_end( library_path_ref );

				if( not os_folder_exists( library_path ) )
				{
					byte const ref default_inputs[] =
						{
							libraries_bytes,
							altar_libraries_data[ 0 ].name,
							altar.file_entries[ setup_libraries_index ].data.elements[ element ]
						};

					TUI_click_buttons( default_inputs, size_of_array( default_inputs ) );
				}

				bytes_paste( setup_libraries[ element ], library_name );
			}
		}

		#if OS_WINDOWS
			bytes_paste_move( new_path_ref, "icon.png" );
			bytes_end( new_path_ref );
			flag const has_icon = os_file_exists( new_path );
			new_path_ref = base_path_ref;

			flag const has_resources = has_icon or has_properties;

			// properties.rc
			{
				byte properties_bytes[ KB( 1 ) ];
				byte ref properties_bytes_ref = properties_bytes;

				if( has_icon )
				{
					bytes_paste_move( properties_bytes_ref, "1 ICON \"icon.ico\"" newline newline );
				}

				if( has_properties )
				{
					byte properties_version[ 32 ];
					byte ref properties_version_ref = properties_version;
					bytes_paste_move( properties_version_ref, altar.file_entries[ setup_properties_index ].data.elements[ 1 ] );
					bytes_paste_move( properties_version_ref, ",0" );
					byte ref version_comma_ref = properties_version;
					while( version_comma_ref < properties_version_ref )
					{
						if( val_of( version_comma_ref ) is '.' )
						{
							val_of( version_comma_ref ) = ',';
						}
						++version_comma_ref;
					}
					bytes_end( properties_version_ref );

					bytes_paste_move( properties_bytes_ref, "1 VERSIONINFO" newline );
					bytes_paste_move( properties_bytes_ref, "FILEVERSION " );
					bytes_paste_move( properties_bytes_ref, properties_version );
					bytes_newline_move( properties_bytes_ref );
					bytes_paste_move( properties_bytes_ref, "PRODUCTVERSION " );
					bytes_paste_move( properties_bytes_ref, properties_version );
					bytes_newline_move( properties_bytes_ref );
					bytes_paste_move( properties_bytes_ref, "FILEOS 0x40004L" newline );
					bytes_paste_move( properties_bytes_ref, "FILETYPE 0x1L" newline );
					bytes_paste_move( properties_bytes_ref, "BEGIN" newline );
					bytes_paste_move( properties_bytes_ref, tab "BLOCK \"StringFileInfo\"" newline );
					bytes_paste_move( properties_bytes_ref, tab "BEGIN" newline );
					bytes_paste_move( properties_bytes_ref, tab tab "BLOCK \"040904b0\"" newline );
					bytes_paste_move( properties_bytes_ref, tab tab "BEGIN" newline );

					bytes_paste_move( properties_bytes_ref, tab tab tab "VALUE \"CompanyName\", \"" );
					bytes_paste_move( properties_bytes_ref, altar.file_entries[ setup_properties_index ].data.elements[ 0 ] );
					bytes_paste_move( properties_bytes_ref, "\"" newline );
					bytes_paste_move( properties_bytes_ref, tab tab tab "VALUE \"FileDescription\", \"" );
					bytes_paste_move( properties_bytes_ref, name );
					bytes_paste_move( properties_bytes_ref, "\"" newline );
					bytes_paste_move( properties_bytes_ref, tab tab tab "VALUE \"ProductName\", \"" );
					bytes_paste_move( properties_bytes_ref, name );
					bytes_paste_move( properties_bytes_ref, "\"" newline );
					bytes_paste_move( properties_bytes_ref, tab tab tab "VALUE \"InternalName\", \"" );
					bytes_paste_move( properties_bytes_ref, name );
					bytes_paste_move( properties_bytes_ref, "\"" newline );
					bytes_paste_move( properties_bytes_ref, tab tab tab "VALUE \"OriginalFilename\", \"" );
					bytes_paste_move( properties_bytes_ref, name );
					bytes_paste_move( properties_bytes_ref, ".exe\"" newline );
					bytes_paste_move( properties_bytes_ref, tab tab tab "VALUE \"FileVersion\", \"" );
					bytes_paste_move( properties_bytes_ref, altar.file_entries[ setup_properties_index ].data.elements[ 1 ] );
					bytes_paste_move( properties_bytes_ref, "\"" newline );
					bytes_paste_move( properties_bytes_ref, tab tab tab "VALUE \"ProductVersion\", \"" );
					bytes_paste_move( properties_bytes_ref, altar.file_entries[ setup_properties_index ].data.elements[ 1 ] );
					bytes_paste_move( properties_bytes_ref, "\"" newline );

					bytes_paste_move( properties_bytes_ref, tab tab "END" newline );
					bytes_paste_move( properties_bytes_ref, tab "END" newline );
					bytes_paste_move( properties_bytes_ref, tab "BLOCK \"VarFileInfo\"" newline );
					bytes_paste_move( properties_bytes_ref, tab "BEGIN" newline );
					bytes_paste_move( properties_bytes_ref, tab tab "VALUE \"Translation\", 0x409, 1200" newline );
					bytes_paste_move( properties_bytes_ref, tab "END" newline );
					bytes_paste_move( properties_bytes_ref, "END" newline );
				}

				bytes_end( properties_bytes_ref );
				//
				bytes_paste_move( new_path_ref, "properties.rc" );
				bytes_end( new_path_ref );
				os_file properties = os_create_file( new_path, new_path_ref - new_path );
				os_file_ref_save( ref_of( properties ), properties_bytes, properties_bytes_ref - properties_bytes );
				os_file_ref_close( ref_of( properties ) );
				new_path_ref = base_path_ref;
			}

			// icon.ico
			if( has_icon )
			{
				type( ico_dir )
				{
					n2 reserved;
					n2 type;
					n2 count;
				};

				type( ico_entry )
				{
					n1 w;
					n1 h;
					n1 colors;
					n1 reserved;
					n2 planes;
					n2 bits;
					n4 size;
					n4 offset;
				};

				byte png_path[ path_max_size ];
				byte ref png_path_ref = png_path;
				bytes_end( new_path_ref );
				bytes_paste_move( png_path_ref, new_path );
				bytes_paste_move( png_path_ref, "icon.png" );
				bytes_end( png_path_ref );

				bytes_paste_move( new_path_ref, "icon.ico" );
				bytes_end( new_path_ref );

				os_file png_file = os_map_file( png_path );
				n8 const png_size = png_file.size;
				n1 const ref const png = to( n1 const ref, png_file.mapped_bytes );

				perm n1 const expect[ 16 ] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A, 0x00, 0x00, 0x00, 0x0D, 'I', 'H', 'D', 'R' };
				if( png_size >= 24 and bytes_match( png, expect, 16 ) )
				{
					// ico header
					n4 const header_size = size_of( ico_dir ) + size_of( ico_entry );
					ico_dir dir = make( ico_dir, 0, 1, 1 );
					ico_entry entry = { pick( ( png[ 16 ] | png[ 17 ] | png[ 18 ] ), 0, png[ 19 ] ), pick( ( png[ 20 ] | png[ 21 ] | png[ 22 ] ), 0, png[ 23 ] ), 0, 0, 1, 32, png_size, header_size };

					n4 const ico_size = header_size + png_size;
					byte ico_bytes[ KB( 3 ) ];
					byte ref ico = ico_bytes;
					bytes_copy_move( ico, ref_of( dir ), size_of( ico_dir ) );
					bytes_copy_move( ico, ref_of( entry ), size_of( ico_entry ) );
					bytes_copy_move( ico, png, png_size );

					os_file ico_file = os_create_file( new_path, new_path_ref - new_path );
					os_file_ref_save( ref_of( ico_file ), ico_bytes, ico_size );
					os_file_ref_close( ref_of( ico_file ) );
				}

				os_file_ref_unmap( ref_of( png_file ) );
				new_path_ref = base_path_ref;
			}
		#endif

		// load framework
		os_file framework_altar = os_map_file( framework_path );
		altar_file_parse( framework_altar.mapped_bytes );
		os_file_ref_unmap( ref_of( framework_altar ) );
		n2 const properties_index = altar_file_find( "properties" );
		n2 const libraries_index = altar_file_find( libraries_bytes );
		n2 const os_index = altar_file_find( OS_NAME );

		// find framework libraries
		if( libraries_index isnt n2_max_val )
		{
			iter( library_element, altar.file_entries[ libraries_index ].elements_count )
			{
				byte ref libraries_element_ref = altar.file_entries[ libraries_index ].data.elements[ library_element ];
				byte const ref const library_name = path_get_name( libraries_element_ref );

				byte library_path[ path_max_size ];
				byte ref library_path_ref = library_path;
				bytes_paste_move( library_path_ref, libraries_bytes separator );
				bytes_paste_move( library_path_ref, library_name );
				bytes_end( library_path_ref );

				if( not os_folder_exists( library_path ) )
				{
					byte const ref default_inputs[] =
						{
							libraries_bytes,
							altar_libraries_data[ 0 ].name,
							altar.file_entries[ libraries_index ].data.elements[ library_element ]
						};

					TUI_click_buttons( default_inputs, size_of_array( default_inputs ) );
				}
			}
		}

		byte const include_prefix[] = "-I" path( "..", "..", libraries_bytes ) separator;

		// make compile_flags.txt
		{
			byte compile_flags_bytes[ KB( 1 ) ];
			byte ref compile_flags_bytes_ref = compile_flags_bytes;
			#if OS_WINDOWS
				bytes_paste_move( compile_flags_bytes_ref, "-Wno-pragma-pack" newline "--target=x86_64-w64-mingw32" newline "-I" path( GCC_PATH, "x86_64-w64-mingw32", "include" ) newline );
			#endif
			//
			if( libraries_index isnt n2_max_val )
			{
				iter( library_element, altar.file_entries[ libraries_index ].elements_count )
				{
					bytes_paste_move( compile_flags_bytes_ref, include_prefix );
					bytes_paste_move( compile_flags_bytes_ref, path_get_name( altar.file_entries[ libraries_index ].data.elements[ library_element ] ) );
					bytes_newline_move( compile_flags_bytes_ref );
				}
			}
			bytes_paste_move( compile_flags_bytes_ref, include_prefix );
			bytes_paste_move( compile_flags_bytes_ref, path_get_name( altar.file_entries[ properties_index ].data.elements[ 0 ] ) );
			bytes_newline_move( compile_flags_bytes_ref );
			iter( setup_library_element, setup_libraries_count )
			{
				bytes_paste_move( compile_flags_bytes_ref, include_prefix );
				bytes_paste_move( compile_flags_bytes_ref, setup_libraries[ setup_library_element ] );
				bytes_newline_move( compile_flags_bytes_ref );
			}
			bytes_end( compile_flags_bytes_ref );
			//
			bytes_paste_move( new_path_ref, "compile_flags.txt" );
			bytes_end( new_path_ref );
			os_file compile_flags_txt = os_create_file( new_path, new_path_ref - new_path );
			os_file_ref_save( ref_of( compile_flags_txt ), compile_flags_bytes, compile_flags_bytes_ref - compile_flags_bytes );
			os_file_ref_close( ref_of( compile_flags_txt ) );
			new_path_ref = base_path_ref;
		}

		// tcc command prefix
		byte tcc_prefix[ KB( 1 ) ];
		{
			byte ref tcc_prefix_ref = tcc_prefix;
			bytes_paste_move( tcc_prefix_ref, TCC_EXE " -xc -B" OS_PICK( TCC_PATH, path( TCC_PATH, "win32" ) ) " " );
			bytes_end( tcc_prefix_ref );
		}

		// gcc command prefix
		byte gcc_prefix[ KB( 1 ) ];
		{
			byte ref gcc_prefix_ref = gcc_prefix;
			bytes_paste_move( gcc_prefix_ref, GCC_EXE " -xc " );
			bytes_end( gcc_prefix_ref );
		}

		// tcc/gcc library includes
		byte include_flags[ KB( 1 ) ];
		{
			byte ref include_flags_ref = include_flags;
			//
			if( libraries_index isnt n2_max_val )
			{
				iter( library_element, altar.file_entries[ libraries_index ].elements_count )
				{
					bytes_paste_move( include_flags_ref, include_prefix );
					bytes_paste_move( include_flags_ref, path_get_name( altar.file_entries[ libraries_index ].data.elements[ library_element ] ) );
					bytes_set_move( include_flags_ref, ' ' );
				}
			}
			bytes_paste_move( include_flags_ref, include_prefix );
			bytes_paste_move( include_flags_ref, path_get_name( altar.file_entries[ properties_index ].data.elements[ 0 ] ) );
			bytes_set_move( include_flags_ref, ' ' );
			iter( setup_library_element, setup_libraries_count )
			{
				bytes_paste_move( include_flags_ref, include_prefix );
				bytes_paste_move( include_flags_ref, setup_libraries[ setup_library_element ] );
				bytes_set_move( include_flags_ref, ' ' );
			}
			bytes_end( include_flags_ref );
		}

		// OS link flags
		byte link_flags[ KB( 1 ) ];
		{
			byte ref link_flags_ref = link_flags;
			iter( os_element, altar.file_entries[ os_index ].elements_count )
			{
				bytes_paste_move( link_flags_ref, altar.file_entries[ os_index ].data.elements[ os_element ] );
				bytes_set_move( link_flags_ref, ' ' );
			}
			bytes_end( link_flags_ref );
		}

		byte script_bytes[ KB( 1 ) ];
		byte ref script_bytes_ref = script_bytes;
		bytes_paste_move( script_bytes_ref, OS_PICK( "#!/bin/bash", "@echo off" ) newline );
		byte ref const script_bytes_base_ref = script_bytes_ref;

		#if OS_LINUX
			byte chmod_bytes[ path_max_size ];
			byte ref chmod_bytes_ref = chmod_bytes;
			bytes_paste_move( chmod_bytes_ref, "chmod +x " );
			byte ref const chmod_bytes_base_ref = chmod_bytes_ref;
		#endif

		// compile_run.sh/bat
		{
			bytes_paste_move( script_bytes_ref, tcc_prefix );
			bytes_paste_move( script_bytes_ref, include_flags );
			bytes_paste_move( script_bytes_ref, link_flags );
			bytes_paste_move( script_bytes_ref, "-b -DDEBUG -D_GNU_SOURCE -run \"" );
			bytes_paste_move( script_bytes_ref, name );
			bytes_paste_move( script_bytes_ref, ".h\"" newline );
			bytes_end( script_bytes_ref );
			//
			bytes_paste_move( new_path_ref, separator "compile_run." OS_PICK( "sh", "bat" ) );
			bytes_end( new_path_ref );
			os_file compile_run_script = os_create_file( new_path, new_path_ref - new_path );
			os_file_ref_save( ref_of( compile_run_script ), script_bytes, script_bytes_ref - script_bytes );
			os_file_ref_close( ref_of( compile_run_script ) );
			#if OS_LINUX
				bytes_paste_move( chmod_bytes_ref, new_path );
				bytes_end( chmod_bytes_ref );
				TUI_command( chmod_bytes );
				chmod_bytes_ref = chmod_bytes_base_ref;
			#endif
			new_path_ref = base_path_ref;
			script_bytes_ref = script_bytes_base_ref;
		}

		// compile_debug.sh/bat
		{
			bytes_paste_move( script_bytes_ref, OS_PICK( "rm -f", "del" ) " \"" );
			bytes_paste_move( script_bytes_ref, name );
			bytes_paste_move( script_bytes_ref, "_debug" OS_PICK(, ".exe" ) "\"" OS_PICK(, " >nul 2>&1" ) newline );
			bytes_paste_move( script_bytes_ref, gcc_prefix );
			bytes_paste_move( script_bytes_ref, include_flags );
			bytes_set_move( script_bytes_ref, '\"' );
			bytes_paste_move( script_bytes_ref, name );
			bytes_paste_move( script_bytes_ref, ".h\" " );
			bytes_paste_move( script_bytes_ref, link_flags );
			bytes_paste_move( script_bytes_ref, "-Wunused -Wno-unused-function -Wno-address-of-packed-member -Wno-packed-bitfield-compat -g -O0 -DDEBUG -D_GNU_SOURCE -o \"" );
			bytes_paste_move( script_bytes_ref, name );
			bytes_paste_move( script_bytes_ref, "_debug" OS_PICK(, ".exe" ) "\"" );
			bytes_end( script_bytes_ref );
			//
			bytes_paste_move( new_path_ref, separator "compile_debug." OS_PICK( "sh", "bat" ) );
			bytes_end( new_path_ref );
			os_file compile_debug_script = os_create_file( new_path, new_path_ref - new_path );
			os_file_ref_save( ref_of( compile_debug_script ), script_bytes, script_bytes_ref - script_bytes );
			os_file_ref_close( ref_of( compile_debug_script ) );
			#if OS_LINUX
				bytes_paste_move( chmod_bytes_ref, new_path );
				bytes_end( chmod_bytes_ref );
				TUI_command( chmod_bytes );
				chmod_bytes_ref = chmod_bytes_base_ref;
			#endif
			new_path_ref = base_path_ref;
			script_bytes_ref = script_bytes_base_ref;
		}

		// compile_release.sh/bat
		{
			bytes_paste_move( script_bytes_ref, OS_PICK( "rm -f", "del" ) " \"" );
			bytes_paste_move( script_bytes_ref, name );
			bytes_paste_move( script_bytes_ref, OS_PICK(, ".exe" ) "\"" OS_PICK(, " >nul 2>&1" ) newline );
			bytes_paste_move( script_bytes_ref, OS_PICK( "rm -f", "del" ) " \"" );
			bytes_paste_move( script_bytes_ref, name );
			bytes_paste_move( script_bytes_ref, "_uncompressed" OS_PICK(, ".exe" ) "\"" OS_PICK(, " >nul 2>&1" ) newline );

			#if OS_WINDOWS
				if( has_resources )
				{
					bytes_paste_move( script_bytes_ref, path( "..", "..", tools_bytes, "tinygw", "bin", "windres" ) " properties.rc -o properties.o" newline );
				}
			#endif
			bytes_paste_move( script_bytes_ref, gcc_prefix );
			bytes_paste_move( script_bytes_ref, include_flags );
			bytes_set_move( script_bytes_ref, '\"' );
			bytes_paste_move( script_bytes_ref, name );
			bytes_paste_move( script_bytes_ref, ".h\" " );
			#if OS_WINDOWS
				if( has_resources )
				{
					bytes_paste_move( script_bytes_ref, "-xnone properties.o " );
				}
			#endif
			bytes_paste_move( script_bytes_ref, link_flags );
			bytes_paste_move( script_bytes_ref, "-w -Ofast -march=x86-64-v3 -flto -fno-plt -fipa-pta -s -D_GNU_SOURCE -o \"" );
			bytes_paste_move( script_bytes_ref, name );
			bytes_paste_move( script_bytes_ref, "_uncompressed" PICK( OS_WINDOWS, ".exe" ) "\"" newline );
			#if OS_WINDOWS
				if( has_resources )
				{
					bytes_paste_move( script_bytes_ref, "del properties.o" newline );
				}
			#endif
			bytes_paste_move( script_bytes_ref, path( "..", "..", tools_bytes, tool_upx_bytes, tool_upx_bytes ) " -qq --best -o \"" );
			bytes_paste_move( script_bytes_ref, name );
			bytes_paste_move( script_bytes_ref, PICK( OS_WINDOWS, ".exe" ) "\" \"" );
			bytes_paste_move( script_bytes_ref, name );
			bytes_paste_move( script_bytes_ref, "_uncompressed" PICK( OS_WINDOWS, ".exe" ) "\"" newline );
			bytes_end( script_bytes_ref );
			//
			bytes_paste_move( new_path_ref, separator "compile_release." OS_PICK( "sh", "bat" ) );
			bytes_end( new_path_ref );
			os_file compile_release_script = os_create_file( new_path, new_path_ref - new_path );
			os_file_ref_save( ref_of( compile_release_script ), script_bytes, script_bytes_ref - script_bytes );
			os_file_ref_close( ref_of( compile_release_script ) );
			#if OS_LINUX
				bytes_paste_move( chmod_bytes_ref, new_path );
				bytes_end( chmod_bytes_ref );
				TUI_command( chmod_bytes );
				chmod_bytes_ref = chmod_bytes_base_ref;
			#endif
			new_path_ref = base_path_ref;
			script_bytes_ref = script_bytes_base_ref;
		}

		// make .vscode/settings.json and .vscode/launch.json
		{
			bytes_paste_move( new_path_ref, ".vscode" );
			bytes_end( new_path_ref );
			os_create_folder( new_path );

			bytes_separator_move( new_path_ref );
			byte ref const vscode_base_ref = new_path_ref;
			//
			{
				byte settings_json_bytes[ KB( 1 ) ];
				byte ref settings_json_bytes_ref = settings_json_bytes;
				bytes_paste_move( settings_json_bytes_ref, "{" newline tab "\"name\": \"" );
				bytes_paste_move( settings_json_bytes_ref, name );
				bytes_paste_move( settings_json_bytes_ref, "\"" newline "}" newline );
				bytes_end( settings_json_bytes_ref );
				//
				bytes_paste_move( new_path_ref, "settings.json" );
				bytes_end( new_path_ref );
				os_file settings_json = os_create_file( new_path, new_path_ref - new_path );
				os_file_ref_save( ref_of( settings_json ), settings_json_bytes, settings_json_bytes_ref - settings_json_bytes );
				os_file_ref_close( ref_of( settings_json ) );
				new_path_ref = vscode_base_ref;
			}
			//
			{
				byte launch_json_bytes[ KB( 1 ) ];
				byte ref launch_json_bytes_ref = launch_json_bytes;
				bytes_paste_move( launch_json_bytes_ref, "{" newline tab "\"version\": \"0.2.0\"," newline tab "\"configurations\":" newline tab "[" newline );
				//
				bytes_paste_move( launch_json_bytes_ref, tab tab "{" newline );
				bytes_paste_move( launch_json_bytes_ref, tab tab tab "\"name\": \"debug\"," newline );
				bytes_paste_move( launch_json_bytes_ref, tab tab tab "\"preLaunchTask\": \"compile debug\"," newline );
				bytes_paste_move( launch_json_bytes_ref, tab tab tab "\"request\": \"launch\"," newline );
				bytes_paste_move( launch_json_bytes_ref, tab tab tab "\"type\": \"gdb\"," newline );
				bytes_paste_move( launch_json_bytes_ref, tab tab tab "\"program\": \"${workspaceFolder}/${config:name}_debug\"," newline );
				bytes_paste_move( launch_json_bytes_ref, tab tab tab "//\"args\": [\"\"]," newline );
				bytes_paste_move( launch_json_bytes_ref, tab tab tab "\"cwd\": \"${workspaceFolder}\"," newline );
				bytes_paste_move( launch_json_bytes_ref, tab tab "}" newline tab "]" newline "}" newline );
				//
				bytes_end( launch_json_bytes_ref );
				//
				bytes_paste_move( new_path_ref, "launch.json" );
				bytes_end( new_path_ref );
				os_file launch_json = os_create_file( new_path, new_path_ref - new_path );
				os_file_ref_save( ref_of( launch_json ), launch_json_bytes, launch_json_bytes_ref - launch_json_bytes );
				os_file_ref_close( ref_of( launch_json ) );
				new_path_ref = vscode_base_ref;
			}
			//
			new_path_ref = base_path_ref;
		}

		// make .github/workflows/build.yml
		{
			byte const ref const framework_name = path_get_name( altar.file_entries[ setup_framework_index ].data.elements[ 0 ] );
			flag const is_h_framework = framework_name[ 0 ] is 'H' and framework_name[ 1 ] is eof_byte;

			bytes_paste_move( new_path_ref, ".github" );
			bytes_end( new_path_ref );
			os_create_folder( new_path );

			bytes_separator_move( new_path_ref );
			bytes_paste_move( new_path_ref, "workflows" );
			bytes_end( new_path_ref );
			os_create_folder( new_path );

			bytes_separator_move( new_path_ref );

			byte build_yml_bytes[ KB( 3 ) ];
			byte ref build_yml_bytes_ref = build_yml_bytes;
			bytes_paste_move( build_yml_bytes_ref, "name: build" newline );
			bytes_paste_move( build_yml_bytes_ref, "on: workflow_dispatch" newline );
			bytes_paste_move( build_yml_bytes_ref, "permissions: { contents: write }" newline newline );

			bytes_paste_move( build_yml_bytes_ref, "env:" newline );
			bytes_paste_move( build_yml_bytes_ref, "  PROGRAM_NAME: \"" );
			bytes_paste_move( build_yml_bytes_ref, name );
			bytes_paste_move( build_yml_bytes_ref, "\"" newline );
			bytes_paste_move( build_yml_bytes_ref, "  PROGRAM_VERSION: \"" );
			bytes_copy_move( build_yml_bytes_ref, setup_version, setup_version_size );
			bytes_paste_move( build_yml_bytes_ref, "\"" newline newline );

			bytes_paste_move( build_yml_bytes_ref, "jobs:" newline );
			bytes_paste_move( build_yml_bytes_ref, "  build:" newline );
			bytes_paste_move( build_yml_bytes_ref, "    strategy:" newline );
			bytes_paste_move( build_yml_bytes_ref, "      matrix:" newline );
			bytes_paste_move( build_yml_bytes_ref, "        os: [ubuntu-latest, windows-latest]" newline );
			bytes_paste_move( build_yml_bytes_ref, "    runs-on: ${{ matrix.os }}" newline );
			bytes_paste_move( build_yml_bytes_ref, "    steps:" newline );

			bytes_paste_move( build_yml_bytes_ref, "      - name: setup structure" newline );
			bytes_paste_move( build_yml_bytes_ref, "        uses: actions/checkout@v4" newline );
			bytes_paste_move( build_yml_bytes_ref, "        with:" newline );
			bytes_paste_move( build_yml_bytes_ref, "          path: projects/${{ env.PROGRAM_NAME }}" newline newline );

			bytes_paste_move( build_yml_bytes_ref, "      - name: download altar" newline );
			bytes_paste_move( build_yml_bytes_ref, "        shell: bash" newline );
			bytes_paste_move( build_yml_bytes_ref, "        run: |" newline );
			bytes_paste_move( build_yml_bytes_ref, "          curl -L https://github.com/H-language/altar/releases/latest/download/altar${{ runner.os == 'Windows' && '.exe' || '' }} -O" newline );
			bytes_paste_move( build_yml_bytes_ref, "          ${{ runner.os == 'Linux' && 'chmod +x altar' || '' }}" newline newline );

			if( not is_h_framework )
			{
				bytes_paste_move( build_yml_bytes_ref, "      - name: install altar dependencies (Linux)" newline );
				bytes_paste_move( build_yml_bytes_ref, "        if: runner.os == 'Linux'" newline );
				bytes_paste_move( build_yml_bytes_ref, "        uses: awalsh128/cache-apt-pkgs-action@v1" newline );
				bytes_paste_move( build_yml_bytes_ref, "        with:" newline );
				bytes_paste_move( build_yml_bytes_ref, "          packages: libx11-dev libxpresent-dev libxrender-dev" newline );
				bytes_paste_move( build_yml_bytes_ref, "          version: 1.0" newline newline );
			}

			bytes_paste_move( build_yml_bytes_ref, "      - name: setup project with altar" newline );
			bytes_paste_move( build_yml_bytes_ref, "        shell: bash" newline );
			bytes_paste_move( build_yml_bytes_ref, "        run: |" newline );
			bytes_paste_move( build_yml_bytes_ref, "          ./altar${{ runner.os == 'Windows' && '.exe' || '' }} cli defaults projects setup ${{ env.PROGRAM_NAME }}" newline newline );

			bytes_paste_move( build_yml_bytes_ref, "      - name: compile" newline );
			bytes_paste_move( build_yml_bytes_ref, "        shell: bash" newline );
			bytes_paste_move( build_yml_bytes_ref, "        run: |" newline );
			bytes_paste_move( build_yml_bytes_ref, "          cd projects/${{ env.PROGRAM_NAME }} && ./compile_release.${{ runner.os == 'Windows' && 'bat' || 'sh' }} && cd ../.." newline newline );

			bytes_paste_move( build_yml_bytes_ref, "      - name: upload artifact" newline );
			bytes_paste_move( build_yml_bytes_ref, "        uses: actions/upload-artifact@v4" newline );
			bytes_paste_move( build_yml_bytes_ref, "        with:" newline );
			bytes_paste_move( build_yml_bytes_ref, "          name: ${{ env.PROGRAM_NAME }}-${{ runner.os }}" newline );
			bytes_paste_move( build_yml_bytes_ref, "          path: |" newline );
			bytes_paste_move( build_yml_bytes_ref, "            projects/${{ env.PROGRAM_NAME }}/${{ env.PROGRAM_NAME }}${{ runner.os == 'Windows' && '.exe' || '' }}" newline );
			bytes_paste_move( build_yml_bytes_ref, "            projects/${{ env.PROGRAM_NAME }}/${{ env.PROGRAM_NAME }}_uncompressed${{ runner.os == 'Windows' && '.exe' || '' }}" newline newline );

			bytes_paste_move( build_yml_bytes_ref, "  release:" newline );
			bytes_paste_move( build_yml_bytes_ref, "    needs: build" newline );
			bytes_paste_move( build_yml_bytes_ref, "    runs-on: ubuntu-latest" newline );
			bytes_paste_move( build_yml_bytes_ref, "    steps:" newline );
			bytes_paste_move( build_yml_bytes_ref, "      - uses: actions/download-artifact@v4" newline );
			bytes_paste_move( build_yml_bytes_ref, "        with: { path: artifacts/ }" newline newline );
			bytes_paste_move( build_yml_bytes_ref, "      - uses: softprops/action-gh-release@v2" newline );
			bytes_paste_move( build_yml_bytes_ref, "        with:" newline );
			bytes_paste_move( build_yml_bytes_ref, "          files: artifacts/*/*" newline );
			bytes_paste_move( build_yml_bytes_ref, "          name: \"download ${{ env.PROGRAM_NAME }}\"" newline );
			bytes_paste_move( build_yml_bytes_ref, "          tag_name: \"${{ env.PROGRAM_VERSION }}\"" newline );
			bytes_paste_move( build_yml_bytes_ref, "          body: |" newline );
			bytes_paste_move( build_yml_bytes_ref, "            # version ${{ env.PROGRAM_VERSION }}" newline newline );
			bytes_paste_move( build_yml_bytes_ref, "            download from the appropriate link for your operating system:" newline newline );
			bytes_paste_move( build_yml_bytes_ref, "            ## [${{ env.PROGRAM_NAME }} for Linux](https://github.com/${{ github.repository }}/releases/download/${{ env.PROGRAM_VERSION }}/${{ env.PROGRAM_NAME }})" newline newline );
			bytes_paste_move( build_yml_bytes_ref, "            ## [${{ env.PROGRAM_NAME }} for Windows](https://github.com/${{ github.repository }}/releases/download/${{ env.PROGRAM_VERSION }}/${{ env.PROGRAM_NAME }}.exe)" newline );
			bytes_end( build_yml_bytes_ref );
			bytes_paste_move( new_path_ref, "build.yml" );
			bytes_end( new_path_ref );

			os_file build_yml = os_create_file( new_path, new_path_ref - new_path );
			os_file_ref_save( ref_of( build_yml ), build_yml_bytes, build_yml_bytes_ref - build_yml_bytes );
			os_file_ref_close( ref_of( build_yml ) );

			new_path_ref = base_path_ref;
		}
	}

	// source.h processing
	{
		byte temp_path[ path_max_size ];
		bytes_end( new_path_ref );
		bytes_paste( temp_path, new_path, ( new_path_ref - new_path ) + 1 );

		bytes_paste_move( new_path_ref, name );
		bytes_paste_move( new_path_ref, ".h" );
		bytes_end( new_path_ref );

		// update source.h version
		if( setup_properties_index isnt n2_max_val )
		{
			if( os_file_exists( new_path ) )
			{
				#define version_suffix_count 4

				os_file source = os_map_file( new_path );
				byte const ref source_ref = source.mapped_bytes;

				perm byte buffer[ MB( 1 ) ];
				byte ref buffer_ref = buffer;

				// uppercase project name
				byte upper_name[ 64 ];
				byte ref upper_name_ref = upper_name;
				iter( name_index, name_size )
				{
					bytes_set_move( upper_name_ref, to_upper_case( name[ name_index ] ) );
				}
				bytes_end( upper_name_ref );

				byte define_major[ 128 ];
				byte define_minor[ 128 ];
				byte define_patch[ 128 ];
				byte define_commit[ 128 ];
				byte ref define_major_ref = define_major;
				byte ref define_minor_ref = define_minor;
				byte ref define_patch_ref = define_patch;
				byte ref define_commit_ref = define_commit;

				bytes_paste_move( define_major_ref, "#define " );
				bytes_paste_move( define_major_ref, upper_name );
				bytes_paste_move( define_major_ref, "_VERSION_MAJOR" );
				bytes_end( define_major_ref );

				bytes_paste_move( define_minor_ref, "#define " );
				bytes_paste_move( define_minor_ref, upper_name );
				bytes_paste_move( define_minor_ref, "_VERSION_MINOR" );
				bytes_end( define_minor_ref );

				bytes_paste_move( define_patch_ref, "#define " );
				bytes_paste_move( define_patch_ref, upper_name );
				bytes_paste_move( define_patch_ref, "_VERSION_PATCH" );
				bytes_end( define_patch_ref );

				bytes_paste_move( define_commit_ref, "#define " );
				bytes_paste_move( define_commit_ref, upper_name );
				bytes_paste_move( define_commit_ref, "_VERSION_COMMIT" );
				bytes_end( define_commit_ref );

				byte version_major[ 16 ];
				byte version_minor[ 16 ];
				byte version_patch[ 16 ];
				byte version_commit[ 16 ];
				byte ref version_major_ref = version_major;
				byte ref version_minor_ref = version_minor;
				byte ref version_patch_ref = version_patch;
				byte ref version_commit_ref = version_commit;

				// extract ###.###.###
				byte const ref extract_ref = setup_version - 1;
				while( val_of( ++extract_ref ) isnt '.' ) bytes_set_move( version_major_ref, val_of( extract_ref ) );
				bytes_end( version_major_ref );
				while( val_of( ++extract_ref ) isnt '.' ) bytes_set_move( version_minor_ref, val_of( extract_ref ) );
				bytes_end( version_minor_ref );
				while( val_of( ++extract_ref ) isnt eof_byte ) bytes_set_move( version_patch_ref, val_of( extract_ref ) );
				bytes_end( version_patch_ref );

				// get commit # from "-#-SHA" via git
				{
					byte git_path[ path_max_size ];
					byte ref git_path_ref = git_path;
					bytes_paste_move( git_path_ref, temp_path );
					bytes_paste_move( git_path_ref, ".git" );
					bytes_end( git_path_ref );

					flag commit_parsed = no;

					if( os_folder_exists( git_path ) )
					{
						byte command[ KB( 1 ) ];
						byte ref command_ref = command;
						bytes_paste_move( command_ref, "git -C " );
						bytes_paste_move( command_ref, temp_path );
						bytes_paste_move( command_ref, " describe --tags --long" );
						bytes_end( command_ref );
						os_handle command_handle = command_read_open_silent( command );
						perm byte line[ 128 ];
						byte ref line_ref = line - 1;
						if( os_handle_get_line( line, size_of_bytes( line ), command_handle ) isnt nothing )
						{
							if( not ( line[ 0 ] is 'f' and line[ 1 ] is 'a' ) ) // not "fatal"
							{
								while( val_of( ++line_ref ) isnt '-' and val_of( line_ref ) isnt newline_byte and val_of( line_ref ) isnt eof_byte );

								if( val_of( line_ref ) is '-' )
								{
									// read until next '-'
									while( val_of( ++line_ref ) isnt '-' and val_of( line_ref ) isnt newline_byte and val_of( line_ref ) isnt eof_byte )
									{
										bytes_set_move( version_commit_ref, val_of( line_ref ) );
									}

									if( version_commit_ref isnt version_commit )
									{
										bytes_end( version_commit_ref );
										commit_parsed = yes;
									}
								}
							}
						}
						command_read_close_silent( command_handle );
					}

					if( not commit_parsed )
					{
						bytes_set_move( version_commit_ref, '0' );
						bytes_end( version_commit_ref );
					}
				}

				byte const ref const defines[ version_suffix_count ] = { define_major, define_minor, define_patch, define_commit };
				n2 const define_sizes[ version_suffix_count ] = { define_major_ref - define_major, define_minor_ref - define_minor, define_patch_ref - define_patch, define_commit_ref - define_commit };
				byte const ref const versions[ version_suffix_count ] = { version_major, version_minor, version_patch, version_commit };
				n2 const version_sizes[ version_suffix_count ] = { version_major_ref - version_major, version_minor_ref - version_minor, version_patch_ref - version_patch, version_commit_ref - version_commit };

				n2 current = 0;
				flag dirty = no;

				process_run:
				{
					byte const ref run_start = source_ref;
					while( val_of( source_ref ) isnt '#' and val_of( source_ref ) isnt eof_byte )
					{
						source_ref += 1;
					}

					n4 const run_size = source_ref - run_start;
					if( run_size > 0 )
					{
						bytes_copy_move( buffer_ref, run_start, run_size );
					}

					jump_if( val_of( source_ref ) is eof_byte ) done_parsing;

					flag valid_match = no;
					if( current < version_suffix_count and bytes_match( source_ref, defines[ current ], define_sizes[ current ] ) )
					{
						byte const after = val_of( source_ref + define_sizes[ current ] );
						valid_match = ( after is ' ' or after is newline_byte or after is '\r' or after is eof_byte );
					}

					if( valid_match )
					{
						bytes_copy_move( buffer_ref, defines[ current ], define_sizes[ current ] );
						bytes_set_move( buffer_ref, ' ' );
						bytes_paste_move( buffer_ref, versions[ current ] );

						source_ref += define_sizes[ current ];

						byte const ref line_rest_start = source_ref;
						while( val_of( source_ref ) isnt newline_byte and val_of( source_ref ) isnt '\r' and val_of( source_ref ) isnt eof_byte )
						{
							source_ref += 1;
						}
						n4 const line_rest_size = source_ref - line_rest_start;

						if( not dirty )
						{
							n4 const expected_size = 1 + version_sizes[ current ];
							if( line_rest_size isnt expected_size or val_of( line_rest_start ) isnt ' ' or not bytes_match( line_rest_start + 1, versions[ current ], version_sizes[ current ] ) )
							{
								dirty = yes;
							}
						}

						current += 1;

						if( current is version_suffix_count )
						{
							n4 const remaining = source.size - ( source_ref - source.mapped_bytes );
							bytes_copy_move( buffer_ref, source_ref, remaining );
							jump done_parsing;
						}
						jump process_run;
					}

					bytes_set_move( buffer_ref, '#' );
					source_ref += 1;
					jump process_run;
				}

				done_parsing:
				os_file_ref_unmap( ref_of( source ) );

				if( dirty )
				{
					bytes_end( buffer_ref );
					os_file out_file = os_create_file( new_path, new_path_ref - new_path );
					os_file_ref_save( ref_of( out_file ), buffer, buffer_ref - buffer );
					os_file_ref_close( ref_of( out_file ) );
				}
			}

			new_path_ref = base_path_ref;
		}

		// format
		byte formatter_path[] = path( tools_bytes, tool_formatter_bytes, tool_formatter_bytes OS_PICK(, ".exe" ) );
		if( os_file_exists( formatter_path ) )
		{
			byte command[ KB( 1 ) ];
			byte ref command_ref = command;
			bytes_copy_move( command_ref, formatter_path, size_of_bytes( formatter_path ) );
			bytes_paste_move( command_ref, " \"" );
			bytes_paste_move( command_ref, new_path );
			bytes_set_move( command_ref, '"' );
			bytes_end( command_ref );

			TUI_command( command );
		}

		new_path_ref = base_path_ref;
	}

	TUI_newline();
	TUI_print( "<c>setup: <y>" );
	TUI_print( name );
	TUI_print( " <m>successful!" );
	TUI_update_now();
	altar_sleep_reset();
}

fn altar_project_open()
{
	byte const ref const project_name = TUI.buttons[ TUI.hover_target ].data;

	byte project_path[ path_max_size ];
	byte ref project_path_ref = project_path;
	bytes_paste_move( project_path_ref, "\"" );
	bytes_paste_move( project_path_ref, projects_bytes );
	bytes_separator_move( project_path_ref );
	bytes_paste_move( project_path_ref, project_name );
	bytes_end( project_path_ref );

	if( os_folder_exists( project_path + 1 ) ) // offset the "
	{
		bytes_paste_move( project_path_ref, "\"" );
		bytes_end( project_path_ref );

		byte command[ KB( 1 ) ];
		byte ref command_ref = command;

		if( os_file_exists( path( tools_bytes, tool_vscode_bytes, "bin", OS_PICK( "codium", "codium.cmd" ) ) ) )
		{
			bytes_paste_move( command_ref, path( tools_bytes, tool_vscode_bytes, "bin", OS_PICK( "codium", "codium.cmd" ) ) " " );
		}
		else
		{
			bytes_paste_move( command_ref, "code " );
		}
		bytes_paste_move( command_ref, project_path );
		bytes_end( command_ref );
		TUI_command( command, yes );
	}

	TUI_newline();
	TUI_print( "<c>open: <y>" );
	TUI_print( project_name );
	TUI_print( " <m>successful!" );
	TUI_update_now();
	altar_sleep_reset();
}

fn altar_tool_install()
{
	byte const ref const tool_name = TUI.buttons[ TUI.hover_target ].data;

	with( tool_name[ 0 ] )
	{
		when( 't' ) // tcc
		{
			#if OS_WINDOWS
				if( not os_folder_exists( path( tools_bytes, "tinygw" ) ) )
				{
					TUI_newline();
					TUI_print( "<c>install: <y>tcc <m>requires <y>gcc<m>!" );
					TUI_update_now();
					altar_sleep_back( tools, 2 );
					out;
				}
			#endif

			flag const tcc_exists = os_folder_exists( tools_bytes separator "tinycc" );

			byte tcc_command[ KB( 1 ) ];
			byte ref tcc_command_ref = tcc_command;

			TUI_newline();
			TUI_print( pick( tcc_exists, "<c>updating <y>tcc<m>...", "<c>installing <y>tcc<m>..." ) );
			TUI_newline();
			TUI_print( pick( tcc_exists, "pulling latest...", "cloning repository..." ) );
			TUI_update_now();

			if( tcc_exists )
			{
				bytes_paste_move( tcc_command_ref, "git -C " tools_bytes separator "tinycc fetch && git -C " tools_bytes separator "tinycc reset --hard origin/HEAD" );
			}
			else
			{
				bytes_paste_move( tcc_command_ref, "cd " tools_bytes " && git clone https://github.com/" );
				bytes_paste_move( tcc_command_ref, altar_tools[ altar_tool_tcc ].repo );
				bytes_paste_move( tcc_command_ref, altar_tools[ altar_tool_tcc ].url_suffix );
			}
			bytes_end( tcc_command_ref );
			TUI_command( tcc_command );

			tcc_command_ref = tcc_command;

			TUI_newline();
			TUI_print( "compiling tcc with gcc, please wait..." );
			TUI_update_now();

			bytes_paste_move( tcc_command_ref, "cd " tools_bytes separator "tinycc" PICK( OS_WINDOWS, separator "win32" ) " && " OS_PICK( "CFLAGS=\"", "call build-tcc.bat -c \"" path( "..", "..", "tinygw", "bin", tool_gcc_bytes ) " " ) "-w -Ofast -march=native -mtune=native -flto -funroll-loops -fipa-pta -s\"" OS_PICK( " ./configure && make -j\"$(nproc)\"", "" ) );
			bytes_end( tcc_command_ref );
			TUI_command( tcc_command );

			#if OS_WINDOWS
				tcc_command_ref = tcc_command;
				bytes_paste_move( tcc_command_ref, "cd " tools_bytes separator "tinycc" separator "win32" " && move /y tcc.exe ..\\ >nul && move /y libtcc.dll ..\\ >nul" );
				bytes_end( tcc_command_ref );
				TUI_command( tcc_command );
			#endif

			TUI_newline();
			TUI_print( pick( tcc_exists, "<c>update: <y>tcc <m>successfully updated!", "<c>install: <y>tcc <m>successfully installed!" ) );
			TUI_update_now();
			altar_tools[ altar_tool_tcc ].needs_update = no;
			altar_sleep_reset();
			out;
		}

		//

		#if OS_WINDOWS
			when( 'g' ) // gcc
			{
				os_delete_folder( tools_bytes separator "tinygw" );

				byte gcc_command[ KB( 1 ) ];
				byte ref gcc_command_ref = gcc_command;

				TUI_newline();
				TUI_print( "<c>installing <y>gcc<m>..." );
				TUI_update_now();

				if( not os_file_exists( path( tools_bytes, "7zr", "7zr.exe" ) ) )
				{
					os_create_folder( tools_bytes separator "7zr" );

					TUI_newline();
					TUI_print( "downloading 7zip..." );
					TUI_update_now();

					bytes_paste_move( gcc_command_ref, altar.curl_path );
					bytes_paste_move( gcc_command_ref, " -s -L -o " path( tools_bytes, "7zr", "7zr.exe" ) " https://github.com/ip7z/7zip/releases/download/25.01/7zr.exe" );
					bytes_end( gcc_command_ref );
					TUI_command( gcc_command );

					gcc_command_ref = gcc_command;
				}
				else
				{
					TUI_newline();
					TUI_print( "using cached 7zip..." );
					TUI_update_now();
				}

				TUI_newline();
				TUI_print( "downloading archive..." );
				TUI_update_now();

				bytes_paste_move( gcc_command_ref, altar.curl_path );
				bytes_paste_move( gcc_command_ref, " -L -o " tools_bytes separator ".tinygw.7z https://github.com/" );
				bytes_paste_move( gcc_command_ref, altar_tools[ altar_tool_gcc ].repo );
				bytes_paste_move( gcc_command_ref, altar_tools[ altar_tool_gcc ].url_prefix );
				bytes_paste_move( gcc_command_ref, altar_tools[ altar_tool_gcc ].version );
				bytes_paste_move( gcc_command_ref, altar_tools[ altar_tool_gcc ].url_mid );
				bytes_paste_move( gcc_command_ref, altar_tools[ altar_tool_gcc ].version );
				bytes_paste_move( gcc_command_ref, altar_tools[ altar_tool_gcc ].url_suffix );
				bytes_end( gcc_command_ref );
				TUI_command( gcc_command );

				gcc_command_ref = gcc_command;

				TUI_newline();
				TUI_print( "extracting archive, please wait..." );
				TUI_update_now();

				bytes_paste_move( gcc_command_ref, path( tools_bytes, "7zr", "7zr.exe" ) " x -aoa " tools_bytes separator ".tinygw.7z -o" tools_bytes );
				bytes_end( gcc_command_ref );
				TUI_command( gcc_command );

				gcc_command_ref = gcc_command;

				TUI_newline();
				TUI_print( "cleaning up..." );
				TUI_update_now();

				bytes_paste_move( gcc_command_ref, "del " tools_bytes separator ".tinygw.7z" );
				bytes_end( gcc_command_ref );
				TUI_command( gcc_command );

				TUI_newline();
				TUI_print( "<c>install: <y>gcc <m>successfully installed!" );
				TUI_update_now();
				altar_tools[ altar_tool_gcc ].needs_update = no;
				altar_sleep_reset();
				out;
			}
		#endif

		//

		when( 'f' ) // formatter
		{
			os_delete_folder( tools_bytes separator tool_formatter_bytes );
			os_create_folder( tools_bytes separator tool_formatter_bytes );

			byte formatter_command[ KB( 1 ) ];
			byte ref formatter_command_ref = formatter_command;

			TUI_newline();
			TUI_print( "<c>installing <y>formatter<m>..." );
			TUI_update_now();

			TUI_newline();
			TUI_print( "downloading formatter..." );
			TUI_update_now();

			bytes_paste_move( formatter_command_ref, altar.curl_path );
			bytes_paste_move( formatter_command_ref, " -L -o " path( tools_bytes, tool_formatter_bytes, tool_formatter_bytes ) OS_PICK( "", ".exe" ) " https://github.com/" );
			bytes_paste_move( formatter_command_ref, altar_tools[ altar_tool_formatter ].repo );
			bytes_paste_move( formatter_command_ref, altar_tools[ altar_tool_formatter ].url_prefix );
			bytes_paste_move( formatter_command_ref, altar_tools[ altar_tool_formatter ].version );
			bytes_paste_move( formatter_command_ref, altar_tools[ altar_tool_formatter ].url_mid );
			bytes_paste_move( formatter_command_ref, altar_tools[ altar_tool_formatter ].url_suffix );
			bytes_end( formatter_command_ref );
			TUI_command( formatter_command );

			#if OS_LINUX
				formatter_command_ref = formatter_command;
				bytes_paste_move( formatter_command_ref, "chmod +x " path( tools_bytes, tool_formatter_bytes, tool_formatter_bytes ) );
				bytes_end( formatter_command_ref );
				TUI_command( formatter_command );
			#endif

			TUI_newline();
			TUI_print( "<c>install: <y>formatter <m>successfully installed!" );
			TUI_update_now();
			altar_tools[ altar_tool_formatter ].needs_update = no;
			altar_sleep_reset();
			out;
		}

		//

		when( 'u' ) // upx
		{
			os_delete_folder( tools_bytes separator tool_upx_bytes );
			os_create_folder( tools_bytes separator tool_upx_bytes );

			byte upx_command[ KB( 1 ) ];
			byte ref upx_command_ref = upx_command;

			TUI_newline();
			TUI_print( "<c>installing <y>upx<m>..." );
			TUI_update_now();

			#if OS_WINDOWS
				if( not os_file_exists( path( tools_bytes, "7zr", "7zr.exe" ) ) )
				{
					os_create_folder( tools_bytes separator "7zr" );

					TUI_newline();
					TUI_print( "downloading 7zip..." );
					TUI_update_now();

					bytes_paste_move( upx_command_ref, altar.curl_path );
					bytes_paste_move( upx_command_ref, " -s -L -o " path( tools_bytes, "7zr", "7zr.exe" ) " https://github.com/ip7z/7zip/releases/download/25.01/7zr.exe" );
					bytes_end( upx_command_ref );
					TUI_command( upx_command );

					upx_command_ref = upx_command;
				}
				else
				{
					TUI_newline();
					TUI_print( "using cached 7zip..." );
					TUI_update_now();
				}

				TUI_newline();
				TUI_print( "downloading and extracting 7zip extras, please wait..." );
				TUI_update_now();

				bytes_paste_move( upx_command_ref, altar.curl_path );
				bytes_paste_move( upx_command_ref, " -s -L -o " tools_bytes separator ".7z-extra.7z https://github.com/ip7z/7zip/releases/download/25.01/7z2501-extra.7z" );
				bytes_end( upx_command_ref );
				TUI_command( upx_command );

				upx_command_ref = upx_command;

				bytes_paste_move( upx_command_ref, path( tools_bytes, "7zr", "7zr.exe" ) " x -aoa " tools_bytes separator ".7z-extra.7z -o" tools_bytes separator ".7z" );
				bytes_end( upx_command_ref );
				TUI_command( upx_command );

				upx_command_ref = upx_command;
			#endif

			TUI_newline();
			TUI_print( "downloading upx..." );
			TUI_update_now();

			bytes_paste_move( upx_command_ref, altar.curl_path );
			bytes_paste_move( upx_command_ref, " -s -L -o " tools_bytes separator ".upx" );
			bytes_paste_move( upx_command_ref, altar_tools[ altar_tool_upx ].url_suffix );
			bytes_paste_move( upx_command_ref, " https://github.com/" );
			bytes_paste_move( upx_command_ref, altar_tools[ altar_tool_upx ].repo );
			bytes_paste_move( upx_command_ref, altar_tools[ altar_tool_upx ].url_prefix );
			bytes_paste_move( upx_command_ref, altar_tools[ altar_tool_upx ].version );
			bytes_paste_move( upx_command_ref, altar_tools[ altar_tool_upx ].url_mid );
			bytes_paste_move( upx_command_ref, altar_tools[ altar_tool_upx ].version );
			bytes_paste_move( upx_command_ref, altar_tools[ altar_tool_upx ].url_suffix );
			bytes_end( upx_command_ref );
			TUI_command( upx_command );

			upx_command_ref = upx_command;

			TUI_newline();
			TUI_print( "extracting upx..." );
			TUI_update_now();

			#if OS_LINUX
				bytes_paste_move( upx_command_ref, "tar -xJf " tools_bytes separator ".upx" );
				bytes_paste_move( upx_command_ref, altar_tools[ altar_tool_upx ].url_suffix );
				bytes_paste_move( upx_command_ref, " -C " tools_bytes separator "upx --strip-components=1" );
			#else
				bytes_paste_move( upx_command_ref, path( tools_bytes, ".7z", "x64", "7za.exe" ) " e -aoa " tools_bytes separator ".upx" );
				bytes_paste_move( upx_command_ref, altar_tools[ altar_tool_upx ].url_suffix );
				bytes_paste_move( upx_command_ref, " -o" tools_bytes separator tool_upx_bytes );
			#endif
			bytes_end( upx_command_ref );
			TUI_command( upx_command );

			upx_command_ref = upx_command;

			TUI_newline();
			TUI_print( "cleaning up..." );
			TUI_update_now();

			#if OS_WINDOWS
				bytes_paste_move( upx_command_ref, "del " tools_bytes separator ".7z-extra.7z && rmdir /s /q " tools_bytes separator ".7z && rmdir /s /q " path( tools_bytes, tool_upx_bytes, "upx-" ) );
				bytes_paste_move( upx_command_ref, altar_tools[ altar_tool_upx ].version );
				bytes_paste_move( upx_command_ref, "-win64 && del " tools_bytes separator ".upx" );
				bytes_paste_move( upx_command_ref, altar_tools[ altar_tool_upx ].url_suffix );
			#else
				bytes_paste_move( upx_command_ref, "rm " tools_bytes separator ".upx" );
				bytes_paste_move( upx_command_ref, altar_tools[ altar_tool_upx ].url_suffix );
			#endif
			bytes_end( upx_command_ref );
			TUI_command( upx_command );

			TUI_newline();
			TUI_print( "<c>install: <y>upx <m>successfully installed!" );
			TUI_update_now();
			altar_tools[ altar_tool_upx ].needs_update = no;
			altar_sleep_reset();
			out;
		}

		//

		when( 'v' ) // vscode
		{
			os_delete_folder( tools_bytes separator tool_vscode_bytes );
			os_create_folder( tools_bytes separator tool_vscode_bytes );

			byte vscode_command[ KB( 1 ) ];
			byte ref vscode_command_ref = vscode_command;

			TUI_newline();
			TUI_print( "<c>installing <y>vscode<m>..." );
			TUI_update_now();

			#if OS_WINDOWS
				if( not os_file_exists( path( tools_bytes, "7zr", "7zr.exe" ) ) )
				{
					os_create_folder( tools_bytes separator "7zr" );

					TUI_newline();
					TUI_print( "downloading 7zip..." );
					TUI_update_now();

					bytes_paste_move( vscode_command_ref, altar.curl_path );
					bytes_paste_move( vscode_command_ref, " -s -L -o " path( tools_bytes, "7zr", "7zr.exe" ) " https://github.com/ip7z/7zip/releases/download/25.01/7zr.exe" );
					bytes_end( vscode_command_ref );
					TUI_command( vscode_command );
					vscode_command_ref = vscode_command;
				}
				else
				{
					TUI_newline();
					TUI_print( "using cached 7zip..." );
					TUI_update_now();
				}

				TUI_newline();
				TUI_print( "downloading and extracting 7zip extras, please wait..." );
				TUI_update_now();

				bytes_paste_move( vscode_command_ref, altar.curl_path );
				bytes_paste_move( vscode_command_ref, " -s -L -o " tools_bytes separator ".7z-extra.7z https://github.com/ip7z/7zip/releases/download/25.01/7z2501-extra.7z" );
				bytes_end( vscode_command_ref );
				TUI_command( vscode_command );
				vscode_command_ref = vscode_command;

				bytes_paste_move( vscode_command_ref, path( tools_bytes, "7zr", "7zr.exe" ) " x -aoa " tools_bytes separator ".7z-extra.7z -o" tools_bytes separator ".7z" );
				bytes_end( vscode_command_ref );
				TUI_command( vscode_command );
				vscode_command_ref = vscode_command;
			#endif

			TUI_newline();
			TUI_print( "downloading vscode..." );
			TUI_update_now();

			bytes_paste_move( vscode_command_ref, altar.curl_path );
			bytes_paste_move( vscode_command_ref, " -L -o " tools_bytes separator ".vscode" );
			bytes_paste_move( vscode_command_ref, altar_tools[ altar_tool_vscode ].url_suffix );
			bytes_paste_move( vscode_command_ref, " https://github.com/" );
			bytes_paste_move( vscode_command_ref, altar_tools[ altar_tool_vscode ].repo );
			bytes_paste_move( vscode_command_ref, altar_tools[ altar_tool_vscode ].url_prefix );
			bytes_paste_move( vscode_command_ref, altar_tools[ altar_tool_vscode ].version );
			bytes_paste_move( vscode_command_ref, altar_tools[ altar_tool_vscode ].url_mid );
			bytes_paste_move( vscode_command_ref, altar_tools[ altar_tool_vscode ].version );
			bytes_paste_move( vscode_command_ref, altar_tools[ altar_tool_vscode ].url_suffix );
			bytes_end( vscode_command_ref );
			TUI_command( vscode_command );
			vscode_command_ref = vscode_command;

			TUI_newline();
			TUI_print( "extracting vscode, please wait..." );
			TUI_update_now();

			#if OS_LINUX
				bytes_paste_move( vscode_command_ref, "tar -xzf " tools_bytes separator ".vscode" );
				bytes_paste_move( vscode_command_ref, altar_tools[ altar_tool_vscode ].url_suffix );
				bytes_paste_move( vscode_command_ref, " -C " tools_bytes separator "vscode --strip-components=1" );
			#else
				bytes_paste_move( vscode_command_ref, path( tools_bytes, ".7z", "x64", "7za.exe" ) " x -aoa " tools_bytes separator ".vscode" );
				bytes_paste_move( vscode_command_ref, altar_tools[ altar_tool_vscode ].url_suffix );
				bytes_paste_move( vscode_command_ref, " -o" tools_bytes separator tool_vscode_bytes );
			#endif
			bytes_end( vscode_command_ref );
			TUI_command( vscode_command );
			vscode_command_ref = vscode_command;

			TUI_newline();
			TUI_print( "cleaning up..." );
			TUI_update_now();

			#if OS_WINDOWS
				bytes_paste_move( vscode_command_ref, "del " tools_bytes separator ".7z-extra.7z && rmdir /s /q " tools_bytes separator ".7z && del " tools_bytes separator ".vscode" );
				bytes_paste_move( vscode_command_ref, altar_tools[ altar_tool_vscode ].url_suffix );
			#else
				bytes_paste_move( vscode_command_ref, "rm " tools_bytes separator ".vscode" );
				bytes_paste_move( vscode_command_ref, altar_tools[ altar_tool_vscode ].url_suffix );
			#endif
			bytes_end( vscode_command_ref );
			TUI_command( vscode_command );
			vscode_command_ref = vscode_command;

			// portable-mode structure
			TUI_newline();
			TUI_print( "configuring profile..." );
			TUI_update_now();

			os_create_folder( path( tools_bytes, tool_vscode_bytes, "data" ) );
			os_create_folder( path( tools_bytes, tool_vscode_bytes, "data", "user-data" ) );
			os_create_folder( path( tools_bytes, tool_vscode_bytes, "data", "user-data", "User" ) );

			// settings.json
			{
				byte settings_bytes[ KB( 2 ) ];
				byte ref settings_bytes_ref = settings_bytes;
				bytes_paste_move( settings_bytes_ref, "{" newline );
				bytes_paste_move( settings_bytes_ref, tab "\"editor.fontSize\": 18," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"editor.lineHeight\": 20," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"editor.insertSpaces\": false," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"editor.tabSize\": 2," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"editor.detectIndentation\": false," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"editor.inlayHints.enabled\": \"off\"," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"editor.minimap.showSlider\": \"always\"," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"editor.minimap.renderCharacters\": false," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"window.zoomLevel\": -1," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"workbench.activityBar.location\": \"top\"," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"workbench.settings.showAISearchToggle\": false," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"workbench.editor.pinnedTabsOnSeparateRow\": true," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"workbench.editor.tabSizing\": \"shrink\"," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"workbench.editor.wrapTabs\": true," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"terminal.integrated.tabs.enabled\": false," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"terminal.integrated.defaultProfile.windows\": \"Command Prompt\"," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"chat.disableAIFeatures\": true," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"debug.terminal.clearBeforeReusing\": true," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"debug.onTaskErrors\": \"abort\"," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"task.problemMatchers.neverPrompt\": { \"shell\": true }," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"git.openRepositoryInParentFolders\": \"never\"," newline );
				bytes_paste_move( settings_bytes_ref, tab "\"clangd.arguments\": [ \"--header-insertion=never\" ]" newline );
				bytes_paste_move( settings_bytes_ref, "}" newline );
				bytes_end( settings_bytes_ref );

				perm byte const settings_path[] = path( tools_bytes, tool_vscode_bytes, "data", "user-data", "User", "settings.json" );
				os_file settings_file = os_create_file( settings_path, size_of_bytes( settings_path ) );
				os_file_ref_save( ref_of( settings_file ), settings_bytes, settings_bytes_ref - settings_bytes );
				os_file_ref_close( ref_of( settings_file ) );
			}

			// keybindings.json
			{
				byte keybindings_bytes[ KB( 1 ) ];
				byte ref keybindings_bytes_ref = keybindings_bytes;
				bytes_paste_move( keybindings_bytes_ref, "[" newline );
				bytes_paste_move( keybindings_bytes_ref, tab "{ \"key\": \"shift+alt+f\", \"command\": \"workbench.action.tasks.runTask\", \"args\": \"format file\" }," newline );
				bytes_paste_move( keybindings_bytes_ref, tab "{ \"key\": \"f6\", \"command\": \"workbench.action.tasks.runTask\", \"args\": \"compile run\" }," newline );
				bytes_paste_move( keybindings_bytes_ref, tab "{ \"key\": \"f7\", \"command\": \"workbench.action.tasks.runTask\", \"args\": \"compile release\" }" newline );
				bytes_paste_move( keybindings_bytes_ref, "]" newline );
				bytes_end( keybindings_bytes_ref );

				perm byte const keybindings_path[] = path( tools_bytes, tool_vscode_bytes, "data", "user-data", "User", "keybindings.json" );
				os_file keybindings_file = os_create_file( keybindings_path, size_of_bytes( keybindings_path ) );
				os_file_ref_save( ref_of( keybindings_file ), keybindings_bytes, keybindings_bytes_ref - keybindings_bytes );
				os_file_ref_close( ref_of( keybindings_file ) );
			}

			// tasks.json
			{
				byte tasks_bytes[ KB( 2 ) ];
				byte ref tasks_bytes_ref = tasks_bytes;
				bytes_paste_move( tasks_bytes_ref, "{" newline );
				bytes_paste_move( tasks_bytes_ref, tab "\"version\": \"2.0.0\"," newline );
				bytes_paste_move( tasks_bytes_ref, tab "\"tasks\": [" newline );
				bytes_paste_move( tasks_bytes_ref, tab tab "{" newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"label\": \"format file\"," newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"type\": \"process\"," newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"command\": \"${workspaceFolder}/../../tools/formatter/formatter" OS_PICK(, ".exe" ) "\"," newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"args\": [ \"${file}\" ]" newline );
				bytes_paste_move( tasks_bytes_ref, tab tab "}," newline );
				bytes_paste_move( tasks_bytes_ref, tab tab "{" newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"label\": \"compile run\"," newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"type\": \"process\"," newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"command\": \"" OS_PICK( "./compile_run.sh", "compile_run.bat" ) "\"" newline );
				bytes_paste_move( tasks_bytes_ref, tab tab "}," newline );
				bytes_paste_move( tasks_bytes_ref, tab tab "{" newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"label\": \"compile debug\"," newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"type\": \"process\"," newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"command\": \"" OS_PICK( "./compile_debug.sh", "compile_debug.bat" ) "\"" newline );
				bytes_paste_move( tasks_bytes_ref, tab tab "}," newline );
				bytes_paste_move( tasks_bytes_ref, tab tab "{" newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"label\": \"compile release\"," newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"type\": \"shell\"," newline );
				bytes_paste_move( tasks_bytes_ref, tab tab tab "\"command\": \"" OS_PICK( "./compile_release.sh && ./${config:name}", "compile_release.bat && ${config:name}.exe" ) "\"" newline );
				bytes_paste_move( tasks_bytes_ref, tab tab "}" newline );
				bytes_paste_move( tasks_bytes_ref, tab "]" newline );
				bytes_paste_move( tasks_bytes_ref, "}" newline );
				bytes_end( tasks_bytes_ref );

				perm byte const tasks_path[] = path( tools_bytes, tool_vscode_bytes, "data", "user-data", "User", "tasks.json" );
				os_file tasks_file = os_create_file( tasks_path, size_of_bytes( tasks_path ) );
				os_file_ref_save( ref_of( tasks_file ), tasks_bytes, tasks_bytes_ref - tasks_bytes );
				os_file_ref_close( ref_of( tasks_file ) );
			}

			// extensions
			{
				byte extensions_path[ path_max_size ];
				byte ref extensions_path_ref = extensions_path;
				bytes_paste_move( extensions_path_ref, path( tools_bytes, tool_vscode_bytes, "data", "extensions" ) );
				bytes_end( extensions_path_ref );
				byte ref const extensions_path_base_ref = extensions_path_ref;
				os_create_folder( extensions_path );

				// project path
				byte project_path[ path_max_size ];
				byte ref project_path_ref = project_path;
				byte ref project_path_walker = project_path_ref;
				bytes_paste_move( project_path_ref, program.path );
				while( val_of( ++project_path_walker ) isnt eof_byte )
				{
					if( val_of( project_path_walker ) is '\\' )
					{
						val_of( project_path_walker ) = '/';
					}
				}
				path_up_folder( project_path );
				project_path_ref = project_path + bytes_measure( project_path );

				// extensions.json
				{
					byte extensions_json[ KB( 1 ) ];
					byte ref extensions_json_ref = extensions_json;
					bytes_paste_move( extensions_json_ref, "[{\"identifier\":{\"id\":\"local.gdb\"},\"version\":\"0.1.0\",\"location\":{\"$mid\":1,\"path\":\"" OS_PICK(, "/" ) );
					bytes_paste_move( extensions_json_ref, project_path );
					bytes_paste_move( extensions_json_ref, "/" tools_bytes "/" tool_vscode_bytes "/data/extensions/gdb" );
					bytes_paste_move( extensions_json_ref, "\",\"scheme\":\"file\"},\"relativeLocation\":\"gdb\"}]" );
					bytes_end( extensions_json_ref );

					bytes_separator_move( extensions_path_ref );
					bytes_paste_move( extensions_path_ref, "extensions.json" );
					bytes_end( extensions_path_ref );
					extensions_path_ref = extensions_path_base_ref;

					os_file extensions_json_file = os_create_file( extensions_path, extensions_path_ref - extensions_path );
					os_file_ref_save( ref_of( extensions_json_file ), extensions_json, extensions_json_ref - extensions_json );
					os_file_ref_close( ref_of( extensions_json_file ) );
				}

				// gdb
				{
					bytes_separator_move( extensions_path_ref );
					bytes_paste_move( extensions_path_ref, "gdb" );
					bytes_end( extensions_path_ref );
					os_create_folder( extensions_path );
					extensions_path_ref = extensions_path_base_ref;

					// package.json
					byte package_json_bytes[ KB( 1 ) ];
					byte ref package_json_bytes_ref = package_json_bytes;
					bytes_paste_move( package_json_bytes_ref, "{" newline );
					bytes_paste_move( package_json_bytes_ref, tab "\"name\": \"gdb\"," newline );
					bytes_paste_move( package_json_bytes_ref, tab "\"publisher\": \"local\"," newline );
					bytes_paste_move( package_json_bytes_ref, tab "\"version\": \"0.1.0\"," newline );
					bytes_paste_move( package_json_bytes_ref, tab "\"engines\": { \"vscode\": \"^1.74.0\" }," newline );
					bytes_paste_move( package_json_bytes_ref, tab "\"contributes\": {" newline );
					bytes_paste_move( package_json_bytes_ref, tab tab "\"debuggers\": [{" newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab "\"type\": \"gdb\"," newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab "\"label\": \"GDB\"," newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab "\"program\": \"" );

					#if OS_WINDOWS
						bytes_paste_move( package_json_bytes_ref, project_path );
						bytes_paste_move( package_json_bytes_ref, "/" tools_bytes "/tinygw/bin/gdb.exe" );
					#else
						{
							os_handle command_handle = command_read_open_silent( "command -v gdb" );
							perm byte gdb_line[ 256 ];
							byte ref gdb_line_ref = gdb_line - 1;
							if( os_handle_get_line( gdb_line, size_of_bytes( gdb_line ), command_handle ) isnt nothing and gdb_line[ 0 ] is '/' )
							{
								while( val_of( ++gdb_line_ref ) isnt newline_byte and val_of( gdb_line_ref ) isnt eof_byte )
								{
									bytes_set_move( package_json_bytes_ref, val_of( gdb_line_ref ) );
								}
							}
							else
							{
								TUI_newline();
								TUI_print( "<y>warning: <m>gdb not found on PATH, falling back to /usr/bin/gdb" );
								TUI_update_now();
								bytes_paste_move( package_json_bytes_ref, "/usr/bin/gdb" );
							}
							command_read_close_silent( command_handle );
						}
					#endif

					bytes_paste_move( package_json_bytes_ref, "\"," newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab "\"args\": [\"-i=dap\"]," newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab "\"configurationAttributes\": {" newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab tab "\"launch\": {" newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab tab tab "\"properties\": {" newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab tab tab tab "\"program\": { \"type\": \"string\" }," newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab tab tab tab "\"args\": { \"type\": \"array\" }," newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab tab tab tab "\"cwd\": { \"type\": \"string\" }" newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab tab tab "}" newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab tab "}" newline );
					bytes_paste_move( package_json_bytes_ref, tab tab tab "}" newline );
					bytes_paste_move( package_json_bytes_ref, tab tab "}]" newline );
					bytes_paste_move( package_json_bytes_ref, tab "}" newline );
					bytes_paste_move( package_json_bytes_ref, "}" );
					bytes_end( package_json_bytes_ref );

					perm byte const package_json_path[] = path( tools_bytes, tool_vscode_bytes, "data", "extensions", "gdb", "package.json" );
					os_file package_json_file = os_create_file( package_json_path, size_of_bytes( package_json_path ) );
					os_file_ref_save( ref_of( package_json_file ), package_json_bytes, package_json_bytes_ref - package_json_bytes );
					os_file_ref_close( ref_of( package_json_file ) );
				}

				// clangd
				bytes_paste_move( vscode_command_ref, OS_PICK( "VSCODE_PORTABLE=\"", "set \"VSCODE_PORTABLE=" ) );
				bytes_paste_move( vscode_command_ref, project_path );
				bytes_paste_move( vscode_command_ref, "/" tools_bytes "/" tool_vscode_bytes "/data" );
				bytes_paste_move( vscode_command_ref, OS_PICK( "\" ", "\" && " ) );

				TUI_newline();
				TUI_print( "installing <y>clangd <m>extension..." );
				TUI_update_now();
				bytes_paste_move( vscode_command_ref, path( tools_bytes, tool_vscode_bytes, "bin", OS_PICK( "codium", "codium.cmd" ) ) " --install-extension llvm-vs-code-extensions.vscode-clangd --force" );
				bytes_end( vscode_command_ref );
				TUI_command( vscode_command );
				vscode_command_ref = vscode_command;
			}

			TUI_newline();
			TUI_print( "<c>install: <y>vscode <m>successfully installed!" );
			TUI_update_now();
			altar_tools[ altar_tool_vscode ].needs_update = no;
			altar_sleep_reset();
			out;
		}

		//

		other skip;
	}
}

#pragma endregion buttons

////////////////
#pragma region | - defaults

fn altar_defaults()
{
	byte const ref default_inputs[] =
		{
			libraries_bytes,
			altar_libraries_data[ 0 ].name,
			"H-language/H",
			libraries_bytes,
			altar_libraries_data[ 0 ].name,
			"H-language/C7H16",
			tools_bytes,
			#if OS_WINDOWS
				tool_gcc_bytes,
				tools_bytes,
			#endif
			tool_upx_bytes
		};

	TUI_click_buttons( default_inputs, size_of_array( default_inputs ) );
}

#pragma endregion defaults

////////////////
#pragma region | - print

fn altar_print_data( fn_ref( anon, fn_data ), byte const data_bytes[][ path_max_size ], n1 const data_count )
{
	n2 longest = 0;
	n2 total = 2 + n2_max( 0, ( data_count - 1 ) * 2 );
	iter( index, data_count )
	{
		n2 const size = bytes_measure( data_bytes[ index ] );
		if( size > longest )
		{
			longest = size;
		}
		total += size;
	}

	flag const single_line = total <= TUI_max_line_size - 32;
	n2 const cell_width = longest + 2;
	n2 const columns = n2_max( 1, ( TUI_max_line_size / cell_width ) - 2 );

	iter( project_id, data_count )
	{
		flag const isnt_last = project_id < data_count - 1;

		TUI_print( "<w>" );
		TUI_print_button( data_bytes[ project_id ], fn_data, data_bytes[ project_id ] );
		if( isnt_last )
		{
			TUI_print( "<c>," );
		}

		if( single_line is no )
		{
			n2 pad = cell_width - bytes_measure( data_bytes[ project_id ] ) - pick( isnt_last, 1, 0 );
			repeat( pad )
			{
				TUI_print( " " );
			}
			TUI_print( "<c>" );
			if( isnt_last and ( ( project_id + 1 ) mod columns ) is 0 )
			{
				TUI_newline();
				TUI_print( "| " );
			}
		}
		else if( isnt_last )
		{
			TUI_print( " " );
		}
		else
		{
			TUI_print( "<c>" );
		}
	}
}

fn altar_print()
{
	flag show_defaults_button = no;
	if( not os_folder_exists( libraries_bytes ) and not os_folder_exists( tools_bytes ) )
	{
		TUI_print( "<c>first time using <m>altar<c>? <y>type/click: <w>" );
		TUI_print_button( "defaults", altar_defaults );
		TUI_newline();
		show_defaults_button = yes;
	}

	altar_ui_item const ref sub_items = nothing;
	i1 sub_count = 0;

	if( altar.selected_top is 0 )
	{
		sub_items = altar_libraries_data;
		sub_count = size_of_array( altar_libraries_data );
	}
	else if( altar.selected_top is 1 )
	{
		sub_items = altar_projects_data;
		sub_count = size_of_array( altar_projects_data );
	}

	//

	TUI_print( " <y>. <c>*  <w>C   <m>__ <y>` <m>__   <c>." newline );
	TUI_print( "  <m>____ <c>. <m>/ / _/ /_<y>*<m>____ <y>` <m>___" newline );
	TUI_print( " <m>_\\__ \\ / / /  __/_\\__ \\ / _ \\" newline );
	TUI_print( "<m>/ __  // /_ / /_ / __  // //_/" newline );
	TUI_print( "<m>\\____/ \\___\\\\___\\\\____//_/" newline );
	TUI_print( "<m>______________/" newline );

	//

	_altar_print_bar( altar_top_data, size_of_array( altar_top_data ), altar.selected_top, "<y>", "<m>" );
	TUI_print( "  " );
	TUI_print_button( "exit", altar_exit );
	TUI_newline();

	if( altar.selected_top >= 0 )
	{
		TUI_print( "<y>" );
		_altar_print_under( altar.selected_top + show_defaults_button );
		TUI_newline();
	}

	if( sub_items isnt nothing )
	{
		_altar_print_bar( sub_items, sub_count, altar.selected_sub, "<c>", "<y>" );
		TUI_newline();

		if( altar.selected_sub >= 0 )
		{
			TUI_print( "<c>" );
			_altar_print_under( size_of_array( altar_top_data ) + 1 + altar.selected_sub + show_defaults_button );
			TUI_print( "\n| " );
		}
	}

	fn_ref( anon, fn_project ) = altar_project_open;

	with( altar.state )
	{
		when( altar_state_libraries_clone )
		{
			TUI_print( "'<m>USERNAME<w>/<m>REPO<c>':" );
			TUI_print_button( nothing, altar_library_clone, TUI.input_bytes );
			TUI_newline();
			skip;
		}
		when( altar_state_libraries_pull )
		{
			TUI_print( "<w>" );
			TUI_print_button( "all", altar_library_pull, "all" );
			TUI_newline();
			TUI_print( "<c>| <w>" );

			altar_print_data( altar_library_pull, altar.libraries, altar.libraries_count );

			TUI_newline();
			skip;
		}

		when( altar_state_projects_new )
		{
			TUI_print( "name:" );
			TUI_print_button( nothing, altar_project_new, TUI.input_bytes );
			TUI_newline();
			skip;
		}
		when( altar_state_projects_setup )
		{
			fn_project = altar_project_setup;
		}
		when( altar_state_projects_open )
		{
			altar_print_data( fn_project, altar.projects, altar.projects_count );

			TUI_newline();
			skip;
		}

		when( altar_state_tools )
		{
			n1 update_count = 0;
			iter( tool_id, altar_tools_count )
			{
				if( altar_tools[ tool_id ].needs_update ) update_count += 1;
			}
			n1 const install_count = altar_tools_count - update_count;

			#define _altar_print_tool_group( LABEL, CONDITION, COUNT, SHOW_VERSION )\
				if( COUNT > 0 )\
				{\
					TUI_print( "<y>| " LABEL ":" );\
					TUI_newline();\
					TUI_print( "| <w>" );\
					n1 printed = 0;\
					iter( tool_id, altar_tools_count )\
					{\
						next_if( not ( CONDITION ) );\
						TUI_print( "<w>" );\
						TUI_print_button( altar_tools[ tool_id ].name, altar_tool_install, altar_tools[ tool_id ].name );\
						if( ( SHOW_VERSION ) and altar_tools[ tool_id ].has_version )\
						{\
							TUI_print( "<c> (" );\
							TUI_print( altar_tools[ tool_id ].version );\
							TUI_print( "!)" );\
						}\
						printed += 1;\
						if( printed < ( COUNT ) )\
						{\
							TUI_print( "<y>, " );\
						}\
					}\
					TUI_newline();\
				}

			_altar_print_tool_group( "update", altar_tools[ tool_id ].needs_update, update_count, yes );
			_altar_print_tool_group( "install", not altar_tools[ tool_id ].needs_update, install_count, no );

			#undef _altar_print_tool_group

			TUI_print( "<y>" );

			skip;
		}
	}

	TUI_print( "> <w>" );
}

#pragma endregion print

#pragma endregion visible

#pragma endregion altar

#pragma endregion definitions

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma region - START
//

start
{
	// version

	if( start_inputs_count is 2 and ( bytes_match( start_inputs[ 1 ], "version\0" ) or bytes_match( start_inputs[ 1 ], "-v\0" ) ) ) // no other inputs
	{
		print( "altar v" ALTAR_VERSION newline );
		exit( success );
	}

	// prerequisites

	if( not system_tool_exists( git ) )
	{
		print( "altar requires git to be installed!" newline );
		exit( failure );
	}

	bytes_paste( altar.curl_path, "curl" );

	if( not system_tool_exists( curl ) )
	{
		#if OS_WINDOWS
			byte const git_curl_path[] = path( "C:", "Program Files", "Git", "mingw64", "bin", "curl.exe" );
			if( os_file_exists( git_curl_path ) )
			{
				altar.curl_path[ 0 ] = '"';
				bytes_paste( altar.curl_path + 1, git_curl_path );
				altar.curl_path[ size_of( git_curl_path ) ] = '"';
				altar.curl_path[ size_of( git_curl_path ) + 1 ] = eof_byte;
			}
			else
		#endif
		{
			print( "altar requires curl to be installed!" newline );
			exit( failure );
		}
	}

	#if OS_LINUX
		if( not system_tool_exists( gcc ) )
		{
			print( "altar requires gcc (and gdb!) to be installed!" newline );
			exit( failure );
		}
	#endif

	// start

	altar.state = altar_state_start;
	altar.selected_top = -1;
	altar.selected_sub = -1;

	TUI_start( ALTAR_NAME, n2x2( 1280, 720 ), 1, altar_print );
}

#pragma endregion start

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
