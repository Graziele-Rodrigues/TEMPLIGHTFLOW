idf_component_register(SRCS "main.c"
                    INCLUDE_DIRS "." 
                    EMBED_TXTFILES ${project_dir}/server_certs/github_cert.pem
)
