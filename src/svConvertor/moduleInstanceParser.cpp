#include <hdlConvertor/svConvertor/moduleInstanceParser.h>

#include <hdlConvertor/notImplementedLogger.h>
#include <hdlConvertor/svConvertor/exprParser.h>
#include <hdlConvertor/svConvertor/paramDefParser.h>
#include <hdlConvertor/svConvertor/typeParser.h>

namespace hdlConvertor {
namespace sv {

using namespace std;
using sv2017Parser = sv2017_antlr::sv2017Parser;
using namespace hdlConvertor::hdlObjects;

VerModuleInstanceParser::VerModuleInstanceParser(
		SVCommentParser &_commentParser) :
		commentParser(_commentParser) {
}

void VerModuleInstanceParser::visitModule_or_interface_or_program_or_udp_instantiation(
		sv2017Parser::Module_or_interface_or_program_or_udp_instantiationContext *ctx,
		vector<unique_ptr<HdlCompInstance>> &res) {
	// module_or_interface_or_program_or_udp_instantiation:
	//     identifier ( parameter_value_assignment )?
	//     hierarchical_instance ( COMMA hierarchical_instance )* SEMI;

	auto module_name = ctx->identifier()->getText();

	auto pva = ctx->parameter_value_assignment();
	vector<unique_ptr<iHdlExpr>> genericMap;
	if (pva) {
		// parameter_value_assignment: HASH LPAREN ( list_of_parameter_value_assignments )? RPAREN;
		auto lpva = pva->list_of_parameter_value_assignments();
		if (lpva)
			genericMap = visitList_of_parameter_value_assignments(lpva);
	}
	auto his = ctx->hierarchical_instance();
	HdlCompInstance *first = nullptr;
	for (auto hi : his) {
		unique_ptr<HdlCompInstance> c = nullptr;
		if (first == nullptr) {
			c = visitHierarchical_instance(hi, iHdlExpr::ID(module_name),
					move(genericMap));
			first = c.get();
		} else {
			auto gm = clone_unique_ptr_vector(first->genericMap);
			c = visitHierarchical_instance(hi, iHdlExpr::ID(module_name),
					move(gm));
		}
		res.push_back(move(c));
	}
}

vector<unique_ptr<iHdlExpr>> VerModuleInstanceParser::visitList_of_parameter_value_assignments(
		sv2017Parser::List_of_parameter_value_assignmentsContext *ctx) {
	// list_of_parameter_value_assignments:
	//     param_expression ( COMMA param_expression )*
	//     | named_parameter_assignment ( COMMA named_parameter_assignment )*
	// ;
	// list_of_parameter_assignments
	//    : ordered_parameter_assignment (',' ordered_parameter_assignment)*
	//    | named_parameter_assignment (',' named_parameter_assignment)*
	//    ;

	vector<unique_ptr<iHdlExpr>> pcs;
	auto pes = ctx->param_expression();
	VerParamDefParser pp(commentParser);
	if (pes.size()) {
		for (auto pe : pes) {
			auto e = pp.visitParam_expression(pe);
			pcs.push_back(move(e));
		}
	} else {
		auto npa = ctx->named_parameter_assignment();
		for (auto pa : npa) {
			// named_parameter_assignment: DOT identifier LPAREN ( param_expression )? RPAREN;
			auto k = VerExprParser::visitIdentifier(pa->identifier());
			unique_ptr<iHdlExpr> v;
			auto e = pa->param_expression();
			if (e) {
				v = pp.visitParam_expression(e);
			} else {
				v = iHdlExpr::null();
			}
			pcs.push_back(
					make_unique<iHdlExpr>(move(k),
							HdlOperatorType::MAP_ASSOCIATION, move(v)));
		}
	}
	return pcs;
}

unique_ptr<HdlCompInstance> VerModuleInstanceParser::visitHierarchical_instance(
		sv2017Parser::Hierarchical_instanceContext *ctx,
		unique_ptr<iHdlExpr> module_id,
		vector<unique_ptr<iHdlExpr>> genericMap) {
	// @note genericMap became the owner of all Expr instances in the vector
	//       that means that instances can not be shared
	// hierarchical_instance: name_of_instance LPAREN list_of_port_connections RPAREN;
	auto noi = ctx->name_of_instance();
	// name_of_instance: identifier ( unpacked_dimension )*;

	auto name = VerExprParser::visitIdentifier(noi->identifier());
	auto uds = noi->unpacked_dimension();
	VerTypeParser tp(commentParser);
	name = tp.applyUnpacked_dimension(move(name), uds);
	auto portMap = visitList_of_port_connections(
			ctx->list_of_port_connections());
	auto c = make_unique<HdlCompInstance>(move(name), move(module_id));
	c->genericMap = move(genericMap);
	c->portMap = move(portMap);
	return c;
}

vector<unique_ptr<iHdlExpr>> VerModuleInstanceParser::visitList_of_port_connections(
		sv2017Parser::List_of_port_connectionsContext *ctx) {
	// list_of_port_connections
	//    : ordered_port_connection (',' ordered_port_connection)*
	//    | named_port_connection (',' named_port_connection)*
	//    ;
	//
	vector<unique_ptr<iHdlExpr>> pcs;
	auto opc = ctx->ordered_port_connection();
	VerExprParser ep(commentParser);
	if (opc.size()) {
		for (auto pc : opc) {
			// ordered_port_connection: ( attribute_instance )* ( expression )?;
			if (pc->attribute_instance().size())
				NotImplementedLogger::print(
						"VerModuleInstanceParser.visitList_of_port_connections.ordered_port_connection attribute_instance",
						pc);
			auto _e = pc->expression();
			unique_ptr<iHdlExpr> e;
			if (_e) {
				e = ep.visitExpression(_e);
			} else {
				e = iHdlExpr::null();
			}
			pcs.push_back(move(e));
		}
	} else {
		auto npc = ctx->named_port_connection();
		for (auto pc : npc) {
			//
			// named_port_connection:
			//  ( attribute_instance )* DOT ( MUL
			//                                | identifier ( LPAREN ( expression )? RPAREN )?
			//                               );
			// named_port_connection
			//    : attribute_instance* '.' identifier '(' (expression)? ')'
			//    ;
			// port_identifier : identifier;
			if (pc->attribute_instance().size())
				NotImplementedLogger::print(
						"VerModuleInstanceParser.visitList_of_port_connections.named_port_connection attribute_instance",
						pc);
			auto _id = pc->identifier();
			unique_ptr<iHdlExpr> k;
			if (_id)
				k = VerExprParser::visitIdentifier(_id);
			else
				k = iHdlExpr::all();
			unique_ptr<iHdlExpr> v;
			auto e = pc->expression();
			if (e) {
				v = ep.visitExpression(e);
			} else {
				v = iHdlExpr::null();
			}
			pcs.push_back(
					make_unique<iHdlExpr>(move(k),
							HdlOperatorType::MAP_ASSOCIATION, move(v)));
		}
	}
	return pcs;
}

}
}
