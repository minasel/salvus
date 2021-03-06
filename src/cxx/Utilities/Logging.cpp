#include <Utilities/Logging.h>
#include <petsc.h>
#include <stdio.h>
Logger::Logger() {}

Logger::~Logger() {

  // catch error case
  if (level == LogLevel::ERROR) {
    std::cerr << "ERROR: " << os.str() << std::endl;
    MPI_Abort(PETSC_COMM_WORLD,-1);
  }
  
  int rank; MPI_Comm_rank(PETSC_COMM_WORLD, &rank);
  if (GLOBAL_LOGGER_STATE.proc == LogProc::ROOTONLY && rank == 0) {        
    // only print at correct verbosity level or higher
    if(GLOBAL_LOGGER_STATE.level >= level) {
      if(GLOBAL_LOGGER_STATE.log_where == LogWhere::STDOUT) {
        // LOG TO STDOUT
        std::cout << os.str() << std::endl;
      }
      else {
        // LOGGING TO FILE
        if (GLOBAL_LOGGER_STATE.output_file.is_open()) {
          GLOBAL_LOGGER_STATE.output_file << os.str() << std::endl;
        }
        else {
          std::cout << "LOGFILE_FAILED, STDOUT INSTEAD: " << os.str() << std::endl;
        } // LOGGING TO FILE
      }      
    }    
  }
  else if (GLOBAL_LOGGER_STATE.proc == LogProc::ALLPROCS) {
    if(level == GLOBAL_LOGGER_STATE.level) {
      std::cout << "proc(" << rank << "): " << os.str() << std::endl;
    }
  }
  
}
