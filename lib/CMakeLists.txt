if(ENABLE_STATIC_AI_LIBS)
	add_main_lib(${VCMI_LIB_TARGET} STATIC)
	target_compile_definitions(${VCMI_LIB_TARGET} PRIVATE STATIC_AI)
	target_link_libraries(${VCMI_LIB_TARGET} PRIVATE
		BattleAI
		EmptyAI
		StupidAI
		VCAI
	)
	if(ENABLE_NULLKILLER_AI)
		target_link_libraries(${VCMI_LIB_TARGET} PRIVATE Nullkiller)
	endif()
else()
	add_main_lib(${VCMI_LIB_TARGET} SHARED)
endif()
if(ENABLE_SINGLE_APP_BUILD)
	target_compile_definitions(${VCMI_LIB_TARGET} PUBLIC VCMI_LIB_NAMESPACE=LIB_CLIENT)
endif()
