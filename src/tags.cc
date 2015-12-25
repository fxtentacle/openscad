#include "tagnode.h"
#include "module.h"
#include "evalcontext.h"
#include "builtin.h"
#include "printutils.h"
#include <sstream>
#include <assert.h>
#include <boost/unordered/unordered_map.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/assign/std/vector.hpp>
#include <boost/assign/list_of.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/foreach.hpp>
#include "stl-utils.h"
#include "expression.h"

using namespace boost::assign; // bring 'operator+=()' into scope

class TagModule : public AbstractModule
{
public:
	TagModule(int type);
	virtual ~TagModule();
	virtual AbstractNode *instantiate(const Context *ctx, const ModuleInstantiation *inst, EvalContext *evalctx) const;
	
private:
	int type;
};

TagModule::TagModule(int type_) 
{
	type = type_;
}

TagModule::~TagModule()
{
}

void selectByTag(std::vector<AbstractNode *>* result, std::vector<AbstractNode *> startingSet, std::set<std::string> filter) {
	BOOST_FOREACH(AbstractNode* child, startingSet) {
		TagNode* tag = dynamic_cast<TagNode*>( child );
		if(!tag || tag->type != 0) {
			selectByTag(result, child->children, filter);
			continue;
		}
		std::list<std::string> missing_tags;
		std::set_difference(filter.begin(), filter.end(), tag->tags.begin(), tag->tags.end(), missing_tags.begin());
		if(!missing_tags.empty()) continue;
		result->push_back(child);
	}
}

void replaceByTag(AbstractNode* currentNode, std::set<std::string> filter, std::string modname, const Context *ctx, EvalContext *evalctx, std::set<std::string> vars) {
	BOOST_FOREACH(AbstractNode* child, currentNode->children) {
		TagNode* tag = dynamic_cast<TagNode*>( child );
		if(!tag || tag->type != 0) {
			replaceByTag(child, filter, modname, ctx, evalctx, vars);
			continue;
		}
		
		std::list<std::string> missing_tags;
		std::set_difference(filter.begin(), filter.end(), tag->tags.begin(), tag->tags.end(), missing_tags.begin());
		if(!missing_tags.empty())  {
			replaceByTag(child, filter, modname, ctx, evalctx, vars);
			continue;
		}		
		
		std::for_each(tag->children.begin(), tag->children.end(), del_fun<AbstractNode>());
		tag->children.clear();
		tag->tags.clear();
		
		ModuleInstantiation* mi = new ModuleInstantiation(modname);
		
		BOOST_FOREACH(std::string var, vars) {
			mi->arguments += Assignment(var, boost::shared_ptr<class Expression>(new ExpressionLookup(var)));
		}
			
		AbstractNode* rr = mi->evaluate(evalctx);
		if(!rr) continue;
		tag->children.push_back( rr );
	}
}

AbstractNode *TagModule::instantiate(const Context *ctx, const ModuleInstantiation *inst, EvalContext *evalctx) const
{
	TagNode *node = new TagNode(inst);
	node->type = type;

	AssignmentList args;
	args += Assignment("set");
	if(type == 2) { 
		args += Assignment("mod");
		args += Assignment("vars");
	}

	Context c(ctx);
	c.setVariables(args, evalctx);
	inst->scope.apply(*evalctx);

	ValuePtr v = c.lookup_variable("set");
	if (v->type() == Value::STRING) {
		std::string tags = v->toString();
		std::istringstream f(tags);
		std::string s;
		while (getline(f, s, ',')) {
			node->tags.insert(s);
		}
	}

	std::vector<AbstractNode *> instantiatednodes = inst->instantiateChildren(evalctx);
	if(type == 1) {
		// do the filtering
		std::vector<AbstractNode *> filtered;
		selectByTag(&filtered, instantiatednodes, node->tags);
		instantiatednodes = filtered;
	}
	node->children.insert(node->children.end(), instantiatednodes.begin(), instantiatednodes.end());

	if(type == 2) {
		ValuePtr modvp = c.lookup_variable("mod");
		std::string modname = "dummy";
		if(modvp->type() == Value::STRING) modname = modvp->toString();
		
		std::set<std::string> vars;
		ValuePtr v = c.lookup_variable("vars");
		if (v->type() == Value::STRING) {
			std::string tags = v->toString();
			std::istringstream f(tags);
			std::string s;
			while (getline(f, s, ',')) {
				vars.insert(s);
			}
		}
		
		replaceByTag(node, node->tags, modname, ctx, evalctx, vars);
	}

	return node;
}

std::string TagNode::toString() const
{
	std::stringstream stream;

	stream << name();
	stream << "(\"" << boost::algorithm::join(tags,", ") << "\")";

	return stream.str();
}

std::string TagNode::name() const
{
	if(type == 0)
		return "tag";
	else if(type == 1)
		return "tagsearch";
	else if(type == 2)
		return "tagreplace";
}

void register_builtin_tags()
{
	Builtins::init("tag", new TagModule(0));
	Builtins::init("tagsearch", new TagModule(1));
	Builtins::init("tagreplace", new TagModule(2));
}
