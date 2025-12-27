#ifndef TML_CLI_CMD_INIT_HPP
#define TML_CLI_CMD_INIT_HPP

namespace tml::cli {

/**
 * Initialize a new TML project
 * Creates tml.toml manifest file
 *
 * @param argc Argument count
 * @param argv Argument values
 * @return 0 on success, non-zero on error
 */
int run_init(int argc, char* argv[]);

} // namespace tml::cli

#endif // TML_CLI_CMD_INIT_HPP
